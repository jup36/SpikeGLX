
#include "CmdServer.h"
#include "Util.h"
#include "MainApp.h"
#include "Version.h"
#include "ConfigCtl.h"
#include "AOCtl.h"
#include "AIQ.h"
#include "Run.h"
#include "Subset.h"
#include "Sha1Verifier.h"
#include "Par2Window.h"

#include <QDir>
#include <QDirIterator>
#include <QThread>


/* ---------------------------------------------------------------- */
/* Statics -------------------------------------------------------- */
/* ---------------------------------------------------------------- */

static QMutex   kilMtx;
static bool     allstop = false;
static void     stopAll()   {QMutexLocker ml(&kilMtx); allstop=true;}
static bool     allStop()   {QMutexLocker ml(&kilMtx); return allstop;}

/* ---------------------------------------------------------------- */
/* class CmdServer ------------------------------------------------ */
/* ---------------------------------------------------------------- */

CmdServer::~CmdServer()
{
    Log() << "Command server stopped.";
}


bool CmdServer::beginListening(
    const QString   &iface,
    ushort          port,
    uint            timeout_ms )
{
    QHostAddress    haddr;

    timeout_msecs = timeout_ms;

    if( iface == "0.0.0.0" )
        haddr = QHostAddress::Any;
    else if( iface == "localhost" || iface == "127.0.0.1" )
        haddr = QHostAddress::LocalHost;
    else
        haddr = iface;

    if( !listen( haddr, port ) ) {
        Error() << QString("CmdSrv could not listen on (%1:%2) [%3].")
                    .arg( haddr.toString() )
                    .arg( port )
                    .arg( errorString() );
        return false;
    }

    Log() << QString("CmdSrv listening on (%1:%2).")
                .arg( haddr.toString() )
                .arg( port );

    return true;
}


void CmdServer::deleteAllActiveConnections()
{
    stopAll();
}


// Create and start a self-destructing connection worker.
//
void CmdServer::incomingConnection( int sockFd )
{
    QThread     *thread = new QThread;
    CmdWorker   *worker = new CmdWorker( sockFd, timeout_msecs );

    worker->moveToThread( thread );

    Connect( thread, SIGNAL(started()), worker, SLOT(run()) );
    Connect( worker, SIGNAL(finished()), worker, SLOT(deleteLater()) );
    Connect( worker, SIGNAL(destroyed()), thread, SLOT(quit()), Qt::DirectConnection );
    Connect( thread, SIGNAL(finished()), thread, SLOT(deleteLater()) );

    thread->start();
}

/* ---------------------------------------------------------------- */
/* class CmdWorker ------------------------------------------------ */
/* ---------------------------------------------------------------- */

CmdWorker::~CmdWorker()
{
    SockUtil::shutdown( sock );

    Debug() << "Del " << SU.tag() << SU.addr();

    if( sock ) {
        delete sock;
        sock = 0;
    }

    if( par2 ) {
        delete par2;
        par2 = 0;
    }
}


void CmdWorker::run()
{
// -----------------------
// Create/configure socket
// -----------------------

    sock = new QTcpSocket;
    sock->setSocketDescriptor( sockFd );
    sock->moveToThread( this->thread() );

    SU.init( sock, timeout, "CmdWorker", &errMsg, true );
    SU.setLowLatency();

    Debug() << "New " << SU.tag() << SU.addr();

// ---------------------
// Get/dispatch messages
// ---------------------

    const int   max_errCt   = 5;
    int         errCt       = 0;

    while( !allStop() && SU.sockValid() && errCt < max_errCt ) {

        QString line = SU.readLine();

        if( line.isNull() )
            break;

        Debug() << "Rcv " << SU.tag() << SU.addr() << " [" << line << "]";

        if( line.length() ) {

            if( processLine( line ) ) {
                sendOK();
                errCt = 0;
            }
            else {
                sendError( errMsg );
                ++errCt;
            }
        }
    }

// ------------
// Self cleanup
// ------------

    Debug() << "End " << SU.tag() << SU.addr();

    emit finished();
}


void CmdWorker::sha1Progress( int pct )
{
    SU.send( QString("%1\n").arg( pct ), true );
}


void CmdWorker::sha1Result( int res )
{
    if( Sha1Worker::Success != res )
        errMsg = "SHA1: Sum does not match sum in meta file.";
}


void CmdWorker::par2Report( const QString &s )
{
    if( s.size() && !SU.send( QString("%1\n").arg( s ), true ) )
        QMetaObject::invokeMethod( par2, "cancel", Qt::QueuedConnection );
}


void CmdWorker::par2Error( const QString &err )
{
    SU.appendError( &errMsg, err );
}


void CmdWorker::sendOK()
{
    SU.send( "OK\n", true );
}


void CmdWorker::sendError( const QString &errMsg )
{
    Error() << "Err " << SU.tag() << SU.addr() << " [" << errMsg << "]";

    if( SU.sockValid() )
        SU.send( QString("ERROR %1\n").arg( errMsg ), true );
}


void CmdWorker::setRunDir( const QString &path )
{
    QFileInfo   info( path );

    if( info.isDir() && info.exists() ) {
        mainApp()->remoteSetsRunDir( path );
        Log() << "Remote client set run dir: " << path;
    }
    else {
        errMsg = QString("'%1' is not a directory or does not exist.")
                    .arg( STR2CHR( path ) );
    }
}


void CmdWorker::enumRunDir()
{
    QString rdir = mainApp()->runDir();
    QDir    dir( rdir );

    if( !dir.exists() )
        errMsg = "Directory does not exist: " + rdir;
    else {

        QDirIterator    it( rdir );

        while( it.hasNext() ) {

            it.next();

            QFileInfo   fi      = it.fileInfo();
            QString     entry   = fi.fileName();

            if( fi.isDir() )
                entry += "/";

            // prepend space to prevent Matlab from barfing
            if( entry.startsWith( "ERROR" ) )
                entry = " " + entry;

            if( !SU.send( QString("%1\n").arg( entry ), true ) )
                break;
        }
    }
}


// Read one param line at a time from client,
// append to str,
// then set params en masse.
//
void CmdWorker::setParams()
{
    if( SU.send( "READY\n", true ) ) {

        QString str, line;

        while( !(line = SU.readLine()).isNull() ) {

            if( !line.length() )
                break; // done on blank line

            str += QString("%1\n").arg( line );
        }

        if( !str.isEmpty() ) {

            QMetaObject::invokeMethod(
                mainApp()->cfgCtl(),
                "cmdSrvSetsParamStr",
                Qt::QueuedConnection,
                Q_ARG(QString, str) );
        }
        else
            errMsg = QString("SETPARAMS: Param string is empty.");
    }
}


// Read one param line at a time from client,
// append to str,
// then set params en masse.
//
void CmdWorker::setAOParams()
{
    if( SU.send( "READY\n", true ) ) {

        QString str, line;

        while( !(line = SU.readLine()).isNull() ) {

            if( !line.length() )
                break; // done on blank line

            str += QString("%1\n").arg( line );
        }

        if( !str.isEmpty() ) {

            QMetaObject::invokeMethod(
                mainApp()->getAOCtl(),
                "cmdSrvSetsAOParamStr",
                Qt::QueuedConnection,
                Q_ARG(QString, str) );
        }
        else
            errMsg = QString("SETAOPARAMS: Param string is empty.");
    }
}


// Expected tok parameter is Boolean 0/1.
//
void CmdWorker::setAOEnable( const QStringList &toks )
{
    if( toks.size() > 0 ) {

        bool    b = toks.front().toInt();

        QMetaObject::invokeMethod(
            mainApp()->getRun(),
            (b ? "aoStart" : "aoStop"),
            Qt::QueuedConnection );
    }
    else
        errMsg = "SETAOENABLE: Requires parameter {0 or 1}.";
}


// Expected tok parameter is Boolean 0/1.
//
void CmdWorker::setTrgEnabled( const QStringList &toks )
{
    if( toks.size() > 0 ) {

        bool    b = toks.front().toInt();

        QMetaObject::invokeMethod(
            mainApp()->getRun(),
            "dfSetTrgEnabled",
            Qt::QueuedConnection,
            Q_ARG(bool, b),
            Q_ARG(bool, true) );
    }
    else
        errMsg = "SETTRGENAB: Requires parameter {0 or 1}.";
}


// Expected tok parameter is run name.
//
void CmdWorker::setRunName( const QStringList &toks )
{
    if( toks.size() > 0 ) {

        QString s = toks.join(" ").trimmed();

        QMetaObject::invokeMethod(
            mainApp(),
            "remoteSetsRunName",
            Qt::QueuedConnection,
            Q_ARG(QString, s) );
    }
    else
        errMsg = "SETRUNNAME: Requires name parameter.";
}


// Expected tok params:
// 0) Boolean 0/1
// 1) channel string
//
void CmdWorker::setDigOut( const QStringList &toks )
{
    if( toks.size() >= 2 ) {

        QString devRem;
        int     lineRem;
        errMsg = CniCfg::parseDIStr( devRem, lineRem, toks.at( 1 ) );

        if( !errMsg.isEmpty() ) {
            Warning() << errMsg;
            return;
        }

        const DAQ::Params  &p = mainApp()->cfgCtl()->acceptedParams;

        if( p.ni.syncEnable ) {

            QString devSync;
            int     lineSync;
            CniCfg::parseDIStr( devSync, lineSync, p.ni.syncLine );

            if( !devSync.compare( devRem, Qt::CaseInsensitive ) ) {
                Warning() <<
                (errMsg = "SETDIGOUT: Cannot use sync line for digital input.");
                return;
            }
        }

        QMetaObject::invokeMethod(
            mainApp(),
            "remoteSetsDigitalOut",
            Qt::QueuedConnection,
            Q_ARG(QString, toks.at( 1 )),
            Q_ARG(bool, toks.at( 0 ).toInt()) );
    }
    else
        Warning() << (errMsg = "SETDIGOUT: Requires at least 2 params.");
}


// Expected tok params:
// 0) starting scan index
// 1) scan count
// 2) <channel subset pattern "id1#id2#...">
// 3) <integer downsample factor>
//
// Send( 'BINARY_DATA %d %d uint64(%ld)'\n", nChans, nScans, headCt ).
// Write binary data stream.
//
void CmdWorker::getNiStreamData( const QStringList &toks )
{
// BK: Decide how remote fetches data from diff streams
// BK: Decided: fetcher per stream

    const AIQ*  niQ = mainApp()->getRun()->getNiQ();

    if( !niQ )
        Warning() << (errMsg = "Not running.");
    else if( toks.size() >= 2 ) {

        const QBitArray &allBits =
                mainApp()->cfgCtl()->acceptedParams.sns.niChans.saveBits;

        QBitArray   chanBits;
        int         nChans  = niQ->NChans();
        uint        dnsmp   = 1;

        // -----
        // Chans
        // -----

        if( toks.size() >= 3 ) {

            QString err =
                Subset::cmdStr2Bits(
                    chanBits, allBits, toks.at( 2 ), nChans );

            if( !err.isEmpty() ) {
                errMsg = err;
                Warning() << err;
                return;
            }
        }
        else
            chanBits = allBits;

        // ----------
        // Downsample
        // ----------

        if( toks.size() >= 4 )
            dnsmp = toks.at( 3 ).toUInt();

        // ---------------------------------
        // Fetch whole timepoints from queue
        // ---------------------------------

        std::vector<AIQ::AIQBlock>  vB;
        quint64                     fromCt  = toks.at( 0 ).toLongLong();
        int                         nMax    = toks.at( 1 ).toInt(),
                                    nb;

        nb = niQ->getNScansFromCt( vB, fromCt, nMax );

        if( nb ) {

            // ----------------
            // Requested subset
            // ----------------

            if( chanBits.count( true ) < nChans ) {

                QVector<uint>   iKeep;

                Subset::bits2Vec( iKeep, chanBits );

                for( int i = 0; i < nb; ++i ) {

                    vec_i16 &D = vB[i].data;

                    Subset::subset( D, D, iKeep, nChans );
                }

                nChans = iKeep.size();
            }

            // ----------
            // Downsample
            // ----------

            if( dnsmp > 1 ) {

                for( int i = 0; i < nb; ++i ) {

                    vec_i16 &D = vB[i].data;

                    Subset::downsample( D, D, nChans, dnsmp );
                }
            }
        }

        // ----
        // Send
        // ----

        if( nb ) {

            vec_i16 cat;
            vec_i16 *data;

            if( niQ->catBlocks( data, cat, vB ) ) {

                SU.send(
                    QString("BINARY_DATA %1 %2 uint64(%3)\n")
                    .arg( nChans )
                    .arg( data->size() / nChans )
                    .arg( vB[0].headCt ),
                    true );

                SU.sendBinary( &(*data)[0], data->size()*sizeof(qint16) );
            }
            else
                Warning() << (errMsg = "GetNiData mem failure.");
        }
        else
            Warning() << (errMsg = "No data read from NI queue.");
    }
    else
        Warning() << (errMsg = "GETDAQDATA: Requires at least 2 params.");
}


void CmdWorker::consoleShow( bool show )
{
    QMetaObject::invokeMethod(
        mainApp(),
        "remoteShowsConsole",
        Qt::QueuedConnection,
        Q_ARG(bool, show) );
}


void CmdWorker::verifySha1( QString file )
{
    mainApp()->makePathAbsolute( file );

    QFileInfo   fi( file );
    KVParams    kvp;

// At this point file, hence fi, could be either
// the binary or meta member of the pair.

    if( fi.isDir() )
        errMsg = "SHA1: Specified path is a directory.";
    else if( !fi.exists() )
        errMsg = "SHA1: Specified file does not exist.";
    else {

        // Here we force fi to be the meta member.

        if( fi.suffix() == "meta" )
            file.clear();
        else {
            fi.setFile(
                QString("%1/%2.meta")
                .arg( fi.path() )
                .arg( fi.completeBaseName() ) );
        }

        if( !fi.exists() )
            errMsg = "SHA1: Required .meta file does not exist.";
        else if( !kvp.fromMetaFile( fi.filePath() ) )
            errMsg = QString("SHA1: Can't read '%1'.").arg( fi.fileName() );
        else {

            if( file.isEmpty() )
                file = kvp["fileName"].toString();

            Sha1Worker  *v = new Sha1Worker( file, kvp );

            Connect( v, SIGNAL(progress(int)), this, SLOT(sha1Progress(int)) );
            Connect( v, SIGNAL(result(int)), this, SLOT(sha1Result(int)) );

            v->run();
            delete v;
        }
    }
}


void CmdWorker::par2Start( QStringList toks )
{
    if( toks.count() >= 2 ) {

        // --------------------
        // Parse command letter
        // --------------------

        Par2Worker::Op  op;

        QString cmd = toks.front().trimmed().toLower();
        toks.pop_front();

        switch( STR2CHR( cmd )[0] ) {

            case 'c':
                op = Par2Worker::Create;
                break;
            case 'v':
                op = Par2Worker::Verify;
                break;
            case 'r':
                op = Par2Worker::Repair;
                break;
            default:
                errMsg =
                    "Error: operation must be"
                    " one of {\"c\", \"v\", \"r\"}";
                return;
        }

        // ---------------
        // Parse file name
        // ---------------

        QString file = toks.join(" ");

        mainApp()->makePathAbsolute( file );

        // ------------------
        // Create par2 worker
        // ------------------

        QThread *thread = new QThread;

        par2 = new Par2Worker( file, op );
        par2->moveToThread( thread );
        Connect( thread, SIGNAL(started()), par2, SLOT(run()) );
        Connect( par2, SIGNAL(report(QString)), this, SLOT(par2Report(QString)) );
        Connect( par2, SIGNAL(error(QString)), this, SLOT(par2Error(QString)) );
        Connect( par2, SIGNAL(finished()), par2, SLOT(deleteLater()) );
        Connect( par2, SIGNAL(destroyed()), thread, SLOT(quit()), Qt::DirectConnection );

        // ----------------
        // Start processing
        // ----------------

        thread->start();

        while( thread->isRunning() )
            guiBreathe();

        // -------
        // Cleanup
        // -------

        delete thread;
        par2 = 0;
    }
    else
        errMsg = "PAR2: Requires at least 2 arguments.";
}


// Return true if cmd handled here.
//
bool CmdWorker::doQuery( const QString &cmd )
{
// --------
// Dispatch
// --------

    QString resp;
    bool    handled = true;

    if( cmd == "GETVERSION" )
        resp = QString("%1\n").arg( VERSION_STR );
    else if( cmd == "ISINITIALIZED" )
        resp = QString("%1\n").arg( mainApp()->isInitialized() );
    else if( cmd == "GETTIME" )
        resp = QString("%1\n").arg( getTime(), 0, 'f', 3 );
    else if( cmd == "GETRUNDIR" )
        resp = QString("%1\n").arg( mainApp()->runDir() );
    else if( cmd == "GETPARAMS" ) {

        QMetaObject::invokeMethod(
            mainApp()->cfgCtl(),
            "cmdSrvGetsParamStr",
            Qt::BlockingQueuedConnection,
            Q_RETURN_ARG(QString, resp) );
    }
    else if( cmd == "GETACQCHANCOUNTS" ) {

        DAQ::Params&p = mainApp()->cfgCtl()->acceptedParams;

        const int   *type = p.ni.niCumTypCnt;

        resp = QString("0 0 %1 %2 %3 %4\n")
                .arg( type[CniCfg::niTypeMN] )
                .arg( type[CniCfg::niTypeMA] - type[CniCfg::niTypeMN] )
                .arg( type[CniCfg::niTypeXA] - type[CniCfg::niTypeMA] )
                .arg( type[CniCfg::niTypeXD] - type[CniCfg::niTypeXA] );
    }
    else if( cmd == "ISRUNNING" )
        resp = QString("%1\n").arg( mainApp()->getRun()->isRunning() );
    else if( cmd == "ISSAVING" )
        resp = QString("%1\n").arg( mainApp()->getRun()->dfIsSaving() );
    else if( cmd == "GETCURRUNFILE" )
        resp = QString("%1\n").arg( mainApp()->getRun()->dfGetCurName() );
    else if( cmd == "GETSCANCOUNT" )
        resp = QString("%1\n").arg( mainApp()->getRun()->getNiScanCount() );
    else if( cmd == "GETCHANNELSUBSET" ) {

        QMetaObject::invokeMethod(
            mainApp()->cfgCtl(),
            "cmdSrvGetsSaveChans",
            Qt::BlockingQueuedConnection,
            Q_RETURN_ARG(QString, resp) );
    }
    else if( cmd == "ISCONSOLEHIDDEN" ) {

        bool    b;

        QMetaObject::invokeMethod(
            mainApp(),
            "remoteGetsIsConsoleHidden",
            Qt::BlockingQueuedConnection,
            Q_RETURN_ARG(bool, b) );

        resp = QString("%1\n").arg( b );
    }
    else
        handled = false;

// -----
// Reply
// -----

    if( handled ) {

        if( resp.isNull() ) {

            if( errMsg.isEmpty() )
                errMsg = "CmdWorker: Application response is empty.";
        }
        else
            SU.send( resp, true );
    }

    return handled;
}


// Return true if cmd handled here.
//
bool CmdWorker::doCommand(
    const QString       &line,
    const QString       &cmd,
    const QStringList   &toks )
{
// --------
// Dispatch
// --------

    bool    handled = true;

    if( cmd == "NOOP" ) {
        // do nothing, will just send OK in caller
    }
    else if( cmd == "SETRUNDIR" )
        setRunDir( line.mid( cmd.length() ).trimmed() );
    else if( cmd == "ENUMRUNDIR" )
        enumRunDir();
    else if( cmd == "SETPARAMS" )
        setParams();
    else if( cmd == "SETAOPARAMS" )
        setAOParams();
    else if( cmd == "SETAOENABLE" )
        setAOEnable( toks );
    else if( cmd == "SETTRGENAB" )
        setTrgEnabled( toks );
    else if( cmd == "SETRUNNAME" )
        setRunName( toks );
    else if( cmd == "STARTRUN" ) {

        QMetaObject::invokeMethod(
            mainApp(),
            "remoteStartsRun",
            Qt::BlockingQueuedConnection,
            Q_RETURN_ARG(QString, errMsg) );
    }
    else if( cmd == "STOPRUN" ) {

        QMetaObject::invokeMethod(
            mainApp(),
            "remoteStopsRun",
            Qt::QueuedConnection );
    }
    else if( cmd == "SETDIGOUT" )
        setDigOut( toks );
    else if( cmd == "FASTSETTLE" )
        Error() << (errMsg = "FASTSETTLE: Not supported.");
    else if( cmd == "GETDAQDATA" )
        getNiStreamData( toks );
    else if( cmd == "CONSOLEHIDE" )
        consoleShow( false );
    else if( cmd == "CONSOLESHOW" )
        consoleShow( true );
    else if( cmd == "VERIFYSHA1" )
        verifySha1( line.mid( cmd.length() ).trimmed() );
    else if( cmd == "PAR2" )
        par2Start( toks );
    else if( cmd == "BYE"
            || cmd == "QUIT"
            || cmd == "EXIT"
            || cmd == "CLOSE" ) {

        Debug() << "Bye " << SU.tag() << SU.addr() << " [Closed by client.]";
        sock->close();
    }
    else
        handled = false;

    return handled;
}


// Some protocol notes:
// --------------------
// Both commands and queries have an errMsg (or evtError)
// box to report problems. Use SocketUtil::appendError()
// to concatenate errors in the order of occurrence. The
// individual error messages should generally not terminate
// with "\n", as that will be applied by CmdWorker::sendError().
//
// If errMsg should become non-empty, the operation has failed.
// On failure this function returns false; no query response is
// returned to the caller; only the errMsg is returned.
//
// No response (resp) is generated or returned for a "command".
//
// For a query, a response is required, and if it is ultimately
// null, an error to that effect is issued. String responses are
// expected to be "\n" terminated, and it is each query handler's
// responsibility to make it so.
//
bool CmdWorker::processLine( const QString &line )
{
// ------------
// Init outputs
// ------------

    errMsg.clear();

// -------------
// Parse command
// -------------

    QStringList toks = line.split(
                        QRegExp("\\s+"),
                        QString::SkipEmptyParts );

    if( toks.empty() ) {
        errMsg = "CmdWorker: Missing command token.";
        return false;
    }

    QString cmd = toks.front().toUpper();
    toks.pop_front();

// --------
// Dispatch
// --------

    if( !doQuery( cmd ) && !doCommand( line, cmd, toks ) )
        errMsg = QString("CmdWorker: Unknown command [%1].").arg( cmd );

    return errMsg.isEmpty();
}


