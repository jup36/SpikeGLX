
#include "TrigSpike.h"
#include "Util.h"
#include "Biquad.h"
#include "MainApp.h"
#include "Run.h"
#include "GraphsWindow.h"

#include <QTimer>
#include <QThread>


#define LOOP_MS     100


static TrigSpike    *ME;


/* ---------------------------------------------------------------- */
/* TrSpkWorker ---------------------------------------------------- */
/* ---------------------------------------------------------------- */

void TrSpkWorker::run()
{
    const int   nID = vID.size();
    bool        ok  = true;

    for(;;) {

        if( !shr.wake( ok ) )
            break;

        for( int iID = 0; iID < nID; ++iID ) {

            if( !(ok = writeSomeIM( vID[iID] )) )
                break;
        }
    }

    emit finished();
}


bool TrSpkWorker::writeSomeIM( int ip )
{
    TrigSpike::CountsIm         &C = ME->imCnt;
    std::vector<AIQ::AIQBlock>  vB;
    int                         nb;

// ---------------------------------------
// Fetch immediately (don't miss old data)
// ---------------------------------------

    if( C.remCt[ip] == -1 ) {
        C.nextCt[ip]  = C.edgeCt[ip] - C.periEvtCt;
        C.remCt[ip]   = 2 * C.periEvtCt + 1;
    }

    nb = imQ[ip]->getNScansFromCt( vB, C.nextCt[ip], C.remCt[ip] );

// ---------------
// Update tracking
// ---------------

    if( !nb )
        return true;

    C.nextCt[ip] = imQ[ip]->nextCt( vB );
    C.remCt[ip] -= C.nextCt[ip] - vB[0].headCt;

// -----
// Write
// -----

    return ME->writeAndInvalVB( ME->DstImec, ip, vB );
}

/* ---------------------------------------------------------------- */
/* TrSpkThread ---------------------------------------------------- */
/* ---------------------------------------------------------------- */

TrSpkThread::TrSpkThread(
    TrSpkShared         &shr,
    const QVector<AIQ*> &imQ,
    QVector<int>        &vID )
{
    thread  = new QThread;
    worker  = new TrSpkWorker( shr, imQ, vID );

    worker->moveToThread( thread );

    Connect( thread, SIGNAL(started()), worker, SLOT(run()) );
    Connect( worker, SIGNAL(finished()), worker, SLOT(deleteLater()) );
    Connect( worker, SIGNAL(destroyed()), thread, SLOT(quit()), Qt::DirectConnection );

    thread->start();
}


TrSpkThread::~TrSpkThread()
{
// worker object auto-deleted asynchronously
// thread object manually deleted synchronously (so we can call wait())

    if( thread->isRunning() )
        thread->wait();

    delete thread;
}

/* ---------------------------------------------------------------- */
/* struct HiPassFnctr --------------------------------------------- */
/* ---------------------------------------------------------------- */

// IMPORTANT!!
// -----------
// Here's the strategy to combat filter transients...
// We look for edges using findFltFallingEdge(), starting from the
// position 'edgeCt'. Every time we modify edgeCt we will tell the
// filter to reset its 'zero' counter to BIQUAD_TRANS_WIDE. We'll
// have the filter zero that many leading data points.

TrigSpike::HiPassFnctr::HiPassFnctr( const DAQ::Params &p )
    :   flt(0), nchans(0), ichan(p.trgSpike.aiChan)
{
    if( p.trgSpike.stream == "nidq" ) {

        if( ichan < p.ni.niCumTypCnt[CniCfg::niSumNeural] ) {

            flt     = new Biquad( bq_type_highpass, 300/p.ni.srate );
            nchans  = p.ni.niCumTypCnt[CniCfg::niSumAll];
            maxInt  = 32768;
        }
    }
    else {

        // Highpass filtering in the Imec AP band is primarily
        // used to remove DC offsets, rather than LFP.

        const CimCfg::AttrEach  &E =
                p.im.each[p.streamID( p.trgSpike.stream )];

        if( ichan < E.imCumTypCnt[CimCfg::imSumAP] ) {

            flt     = new Biquad( bq_type_highpass, 300/p.im.all.srate );
            nchans  = E.imCumTypCnt[CimCfg::imSumAll];
            maxInt  = 512;
        }
    }

    reset();
}


TrigSpike::HiPassFnctr::~HiPassFnctr()
{
    if( flt )
        delete flt;
}


void TrigSpike::HiPassFnctr::reset()
{
    nzero = BIQUAD_TRANS_WIDE;
}


void TrigSpike::HiPassFnctr::operator()( vec_i16 &data )
{
    if( flt ) {

        int ntpts = (int)data.size() / nchans;

        flt->apply1BlockwiseMem1( &data[0], maxInt, ntpts, nchans, ichan );

        if( nzero > 0 ) {

            // overwrite with zeros

            if( ntpts > nzero )
                ntpts = nzero;

            short   *d      = &data[ichan],
                    *dlim   = &data[ichan + ntpts*nchans];

            for( ; d < dlim; d += nchans )
                *d = 0;

            nzero -= ntpts;
        }
    }
}

/* ---------------------------------------------------------------- */
/* TrigSpike ------------------------------------------------------ */
/* ---------------------------------------------------------------- */

TrigSpike::TrigSpike(
    const DAQ::Params   &p,
    GraphsWindow        *gw,
    const QVector<AIQ*> &imQ,
    const AIQ           *niQ )
    :   TrigBase( p, gw, imQ, niQ ),
        usrFlt(new HiPassFnctr( p )),
        imCnt( p, p.im.all.srate ),
        niCnt( p, p.ni.srate ),
        nCycMax(
            p.trgSpike.isNInf ?
            std::numeric_limits<qlonglong>::max()
            : p.trgSpike.nS),
        aEdgeCtNext(0),
        thresh(
            p.trgSpike.stream == "nidq" ?
            p.ni.vToInt16( p.trgSpike.T, p.trgSpike.aiChan )
            : p.im.vToInt10( p.trgSpike.T, p.streamID( p.trgSpike.stream ),
                p.trgSpike.aiChan ))
{
}


void TrigSpike::setGate( bool hi )
{
    runMtx.lock();
    initState();
    baseSetGate( hi );
    runMtx.unlock();
}


void TrigSpike::resetGTCounters()
{
    runMtx.lock();
    baseResetGTCounters();
    initState();
    runMtx.unlock();
}


#define SETSTATE_GetEdge    (state = 0)

#define ISSTATE_GetEdge     (state == 0)
#define ISSTATE_Write       (state == 1)
#define ISSTATE_Done        (state == 2)


// Spike logic is driven by TrgSpikeParams:
// {periEvtSecs, refractSecs, inarow, nS, T}.
// Corresponding states defined above.
//
void TrigSpike::run()
{
    Debug() << "Trigger thread started.";

// ---------
// Configure
// ---------

    ME = this;

// Create worker threads

    const int               nPrbPerThd = 2;

    QVector<TrSpkThread*>   trT;
    TrSpkShared             shr( p );

    nThd = 0;

    for( int ip0 = 0; ip0 < nImQ; ip0 += nPrbPerThd ) {

        QVector<int>    vID;

        for( int id = 0; id < nPrbPerThd; ++id ) {

            if( ip0 + id < nImQ )
                vID.push_back( ip0 + id );
            else
                break;
        }

        trT.push_back( new TrSpkThread( shr, imQ, vID ) );
        ++nThd;
    }

// Wait for threads to reach ready (sleep) state

    shr.runMtx.lock();
        while( shr.asleep < nThd ) {
            shr.runMtx.unlock();
                usleep( 10 );
            shr.runMtx.lock();
        }
    shr.runMtx.unlock();

// -----
// Start
// -----

    setYieldPeriod_ms( LOOP_MS );

    initState();

    while( !isStopped() ) {

        double  loopT = getTime();
        bool    inactive;

        // -------
        // Active?
        // -------

        inactive = ISSTATE_Done || !isGateHi();

        if( inactive )
            goto next_loop;

        // -------------
        // If gate start
        // -------------

        // Set gateHiT as place from which to start
        // searching for edge in getEdge().

        if( !imCnt.edgeCt.size() || !niCnt.edgeCt ) {

            usrFlt->reset();

            double  gateT = getGateHiT();

            quint64 imEdgeCt;
            if( nImQ && !imQ[0]->mapTime2Ct( imEdgeCt, gateT ) )
                goto next_loop;
            imCnt.edgeCt.fill( imEdgeCt, nImQ );

            if( niQ && !niQ->mapTime2Ct( niCnt.edgeCt, gateT ) )
                goto next_loop;
        }

        // --------------
        // Seek next edge
        // --------------

        if( ISSTATE_GetEdge ) {

            quint64 imEdgeCt;

            if( p.trgSpike.stream == "nidq" ) {

                const AIQ   *aiQ = (nImQ ? imQ[0] : 0);

                if( !getEdge( niCnt.edgeCt, niCnt, niQ, imEdgeCt, aiQ ) )
                    goto next_loop;
            }
            else {

                int ip = p.streamID( p.trgSpike.stream );

                imEdgeCt = imCnt.edgeCt[ip];

                if( !getEdge( imEdgeCt, imCnt, imQ[ip], niCnt.edgeCt, niQ ) )
                    goto next_loop;
            }

            imCnt.edgeCt.fill( imEdgeCt, nImQ );

            QMetaObject::invokeMethod(
                gw, "blinkTrigger",
                Qt::QueuedConnection );

            // ---------------
            // Start new files
            // ---------------

            imCnt.remCt.fill( -1, nImQ );
            niCnt.remCt = -1;

            {
                int ig, it;

                if( !newTrig( ig, it, false ) )
                    break;

                setSyncWriteMode();
            }

            SETSTATE_Write();
        }

        // ----------------
        // Handle this edge
        // ----------------

        if( ISSTATE_Write ) {

            if( !xferAll( shr ) )
                goto endrun;

            // -----
            // Done?
            // -----

            if( niCnt.remCt <= 0 && imCnt.remCtDone() ) {

                endTrig();

                usrFlt->reset();
                imCnt.advanceEdgeByRefrac();
                niCnt.edgeCt += niCnt.refracCt;

                if( ++nS >= nCycMax )
                    SETSTATE_Done();
                else
                    SETSTATE_GetEdge;
            }
        }

        // ------
        // Status
        // ------

next_loop:
       if( loopT - statusT > 0.25 ) {

            QString sOn, sWr;
            int     ig, it;

            getGT( ig, it );
            statusOnSince( sOn, loopT, ig, it );
            statusWrPerf( sWr );

            Status() << sOn << sWr;

            statusT = loopT;
        }

        // -------------------
        // Moderate fetch rate
        // -------------------

        yield( loopT );
    }

endrun:
// Kill all threads

    shr.kill();

    for( int iThd = 0; iThd < nThd; ++iThd )
        delete trT[iThd];

// Done

    endRun();

    Debug() << "Trigger thread stopped.";

    emit finished();
}


void TrigSpike::SETSTATE_Write()
{
    state       = 1;
    aEdgeCtNext = 0;
}


void TrigSpike::SETSTATE_Done()
{
    state = 2;
    mainApp()->getRun()->dfSetRecordingEnabled( false, true );
}


void TrigSpike::initState()
{
    usrFlt->reset();
    imCnt.edgeCt.clear();
    niCnt.edgeCt    = 0;
    nS              = 0;
    SETSTATE_GetEdge;
}


// Find edge in stream A...but translate time-points to stream B.
//
// Return true if found edge later than full premargin.
//
// Also require edge a little later (latencyCt) to allow
// time to start fetching those data.
//
bool TrigSpike::getEdge(
    quint64         &aEdgeCt,
    const Counts    &cA,
    const AIQ       *qA,
    quint64         &bEdgeCt,
    const AIQ       *qB )
{
    quint64 minCt = qA->qHeadCt() + cA.periEvtCt + cA.latencyCt;
    bool    found;

    if( aEdgeCt < minCt ) {

        usrFlt->reset();
        aEdgeCt = minCt;
    }

// For multistream, we need mappable data for both A and B.
// aEdgeCtNext saves us from costly refinding of A in cases
// where A already succeeded but B not yet.

    if( aEdgeCtNext )
        found = true;
    else {
        found = qA->findFltFallingEdge(
                    aEdgeCtNext,
                    aEdgeCt,
                    p.trgSpike.aiChan,
                    thresh,
                    p.trgSpike.inarow,
                    *usrFlt );

        if( !found ) {
            aEdgeCt     = aEdgeCtNext;  // pick up search here
            aEdgeCtNext = 0;
        }
    }

    if( found && qB ) {

        double  wallT;

        qA->mapCt2Time( wallT, aEdgeCtNext );
        found = qB->mapTime2Ct( bEdgeCt, wallT );
    }

    if( found ) {

        alignX12( qA, aEdgeCtNext, bEdgeCt );

        aEdgeCt     = aEdgeCtNext;
        aEdgeCtNext = 0;
    }

    return found;
}


bool TrigSpike::writeSomeNI()
{
    if( !niQ )
        return true;

    CountsNi                    &C = niCnt;
    std::vector<AIQ::AIQBlock>  vB;
    int                         nb;

// ---------------------------------------
// Fetch immediately (don't miss old data)
// ---------------------------------------

    if( C.remCt == -1 ) {
        C.nextCt  = C.edgeCt - C.periEvtCt;
        C.remCt   = 2 * C.periEvtCt + 1;
    }

    nb = niQ->getNScansFromCt( vB, C.nextCt, C.remCt );

// ---------------
// Update tracking
// ---------------

    if( !nb )
        return true;

    C.nextCt = niQ->nextCt( vB );
    C.remCt -= C.nextCt - vB[0].headCt;

// -----
// Write
// -----

    return writeAndInvalVB( DstNidq, 0, vB );
}


// Return true if no errors.
//
bool TrigSpike::xferAll( TrSpkShared &shr )
{
    int niOK;

    shr.awake   = 0;
    shr.asleep  = 0;
    shr.errors  = 0;

// Wake all imec threads

    shr.condWake.wakeAll();

// Do nidq locally

    niOK = writeSomeNI();

// Wait all threads started, and all done

    shr.runMtx.lock();
        while( shr.awake  < nThd
            || shr.asleep < nThd ) {

            shr.runMtx.unlock();
                msleep( LOOP_MS/8 );
            shr.runMtx.lock();
        }
    shr.runMtx.unlock();

    return niOK && !shr.errors;
}


