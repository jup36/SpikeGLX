
#include "Util.h"
#include "CimCfg.h"
#include "Subset.h"
#include "SignalBlocker.h"

#ifdef HAVE_IMEC
#include "IMEC/NeuropixAPI.h"
#else
#pragma message("*** Message to self: Building simulated IMEC version ***")
#endif

#include <QBitArray>
#include <QSettings>
#include <QTableWidget>


/* ---------------------------------------------------------------- */
/* struct IMRODesc ------------------------------------------------ */
/* ---------------------------------------------------------------- */

// Pattern: "chn bank refid apgn lfgn apflt"
//
QString IMRODesc::toString( int chn ) const
{
    return QString("%1 %2 %3 %4 %5 %6")
            .arg( chn )
            .arg( bank ).arg( refid )
            .arg( apgn ).arg( lfgn )
            .arg( apflt );
}


// Pattern: "chn bank refid apgn lfgn apflt"
//
// Note: The chn field is discarded.
//
IMRODesc IMRODesc::fromString( const QString &s )
{
    const QStringList   sl = s.split(
                                QRegExp("\\s+"),
                                QString::SkipEmptyParts );

    return IMRODesc(
            sl.at( 1 ).toInt(), sl.at( 2 ).toInt(),
            sl.at( 3 ).toInt(), sl.at( 4 ).toInt(),
            sl.at( 5 ).toInt() );
}

/* ---------------------------------------------------------------- */
/* struct IMROTbl ------------------------------------------------- */
/* ---------------------------------------------------------------- */

void IMROTbl::fillDefault( int type )
{
    this->type = type;

    e.clear();
    e.resize( imType0Chan );
}


// Return true if two tables are same w.r.t banks.
//
bool IMROTbl::banksSame( const IMROTbl &rhs ) const
{
    int n = nChan();

    for( int i = 0; i < n; ++i ) {

        if( e[i].bank != rhs.e[i].bank )
            return false;
    }

    return true;
}


// Pattern: (type,nchan)(chn bank refid apgn lfgn apflt)()()...
//
QString IMROTbl::toString() const
{
    QString     s;
    QTextStream ts( &s, QIODevice::WriteOnly );
    int         n = nChan();

    ts << "(" << type << "," << n << ")";

    for( int i = 0; i < n; ++i )
        ts << "(" << e[i].toString( i ) << ")";

    return s;
}


// Pattern: (type,nchan)(chn bank refid apgn lfgn apflt)()()...
//
void IMROTbl::fromString( const QString &s )
{
    QStringList sl = s.split(
                        QRegExp("^\\s*\\(|\\)\\s*\\(|\\)\\s*$"),
                        QString::SkipEmptyParts );
    int         n  = sl.size();

// Header

    QStringList hl = sl[0].split(
                        QRegExp("^\\s+|\\s*,\\s*"),
                        QString::SkipEmptyParts );

    type = hl[0].toUInt();

// Entries

    e.clear();
    e.reserve( n - 1 );

    for( int i = 1; i < n; ++i )
        e.push_back( IMRODesc::fromString( sl[i] ) );
}


bool IMROTbl::loadFile( QString &msg, const QString &path )
{
    QFile       f( path );
    QFileInfo   fi( path );

    if( !fi.exists() ) {

        msg = QString("Can't find [%1]").arg( fi.fileName() );
        return false;
    }
    else if( f.open( QIODevice::ReadOnly | QIODevice::Text ) ) {

        fromString( f.readAll() );

        if( type == 0 && nChan() == imType0Chan ) {

            msg = QString("Loaded (type=%1) file [%2]")
                    .arg( type )
                    .arg( fi.fileName() );
            return true;
        }
        else {
            msg = QString("Error reading [%1]").arg( fi.fileName() );
            return false;
        }
    }
    else {
        msg = QString("Error opening [%1]").arg( fi.fileName() );
        return false;
    }
}


bool IMROTbl::saveFile( QString &msg, const QString &path )
{
    QFile       f( path );
    QFileInfo   fi( path );

    if( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {

        int n = f.write( STR2CHR( toString() ) );

        if( n > 0 ) {

            msg = QString("Saved (type=%1) file [%2]")
                    .arg( type )
                    .arg( fi.fileName() );
            return true;
        }
        else {
            msg = QString("Error writing [%1]").arg( fi.fileName() );
            return false;
        }
    }
    else {
        msg = QString("Error opening [%1]").arg( fi.fileName() );
        return false;
    }
}

/* ---------------------------------------------------------------- */
/* Statics -------------------------------------------------------- */
/* ---------------------------------------------------------------- */

static int i2gn[IMROTbl::imNGains]  = {50,125,250,500,1000,1500,2000,3000};


int IMROTbl::typeToNElec( int type )
{
    Q_UNUSED( type )

    return imType0Elec;
}


int IMROTbl::chToEl384( int ch, int bank )
{
    return (ch >= 0 ? (ch + 1) + bank*imType0Chan : 0);
}


bool IMROTbl::chIsRef( int ch )
{
    return ch == 191;
}


int IMROTbl::idxToGain( int idx )
{
    return (idx >= 0 && idx < 8 ? i2gn[idx] : i2gn[3]);
}


int IMROTbl::gainToIdx( int gain )
{
    switch( gain ) {
        case 50:
            return 0;
        case 125:
            return 1;
        case 250:
            return 2;
        case 500:
            return 3;
        case 1000:
            return 4;
        case 1500:
            return 5;
        case 2000:
            return 6;
        case 3000:
            return 7;
        default:
            break;
    }

    return 3;
}

/* ---------------------------------------------------------------- */
/* struct IMProbeDat ---------------------------------------------- */
/* ---------------------------------------------------------------- */

void CimCfg::ImProbeDat::load( QSettings &S, int i )
{
    QString row =
        S.value(
            QString("row%1").arg( i ),
            "slot:0 port:0 enab:1" ).toString();

    QStringList sl = row.split(
                        QRegExp("\\s+"),
                        QString::SkipEmptyParts );

    QStringList s;

    s = sl[0].split(
            QRegExp("^\\s+|\\s*:\\s*"),
            QString::SkipEmptyParts );

    slot = s.at( 1 ).toUInt();

    s = sl[1].split(
            QRegExp("^\\s+|\\s*:\\s*"),
            QString::SkipEmptyParts );

    port = s.at( 1 ).toUInt();

    s = sl[2].split(
            QRegExp("^\\s+|\\s*:\\s*"),
            QString::SkipEmptyParts );

    enab = s.at( 1 ).toUInt();
}


void CimCfg::ImProbeDat::save( QSettings &S, int i ) const
{
    S.setValue(
        QString("row%1").arg( i ),
        QString("slot:%1 port:%2 enab:%3")
            .arg( slot ).arg( port ).arg( enab ) );
}

/* ---------------------------------------------------------------- */
/* struct ImProbeTable -------------------------------------------- */
/* ---------------------------------------------------------------- */

void CimCfg::ImProbeTable::init()
{
    for( int i = 0, n = probes.size(); i < n; ++i )
        probes[i].init();

    id2dat.clear();
    slot.clear();

    api.clear();
    slot2Vers.clear();
}


void CimCfg::ImProbeTable::loadSettings()
{
// Load from ini file

    STDSETTINGS( settings, "improbetable" );
    settings.beginGroup( "ImPrbTabUserInput" );

    comIdx = settings.value( "comIdx", 1 ).toInt();
    int np = settings.value( "nrows", 1 ).toInt();

    probes.resize( np );

    for( int i = 0; i < np; ++i )
        probes[i].load( settings, i );

    qSort( probes.begin(), probes.end() );
}


void CimCfg::ImProbeTable::saveSettings() const
{
    STDSETTINGS( settings, "improbetable" );
    settings.remove( "ImPrbTabUserInput" );
    settings.beginGroup( "ImPrbTabUserInput" );

    int np = probes.size();

    settings.setValue( "comIdx", comIdx );
    settings.setValue( "nrows", probes.size() );

    for( int i = 0; i < np; ++i )
        probes[i].save( settings, i );
}


void CimCfg::ImProbeTable::toGUI( QTableWidget *T ) const
{
    SignalBlocker   b0(T);

    int np = probes.size();

    T->setRowCount( np );

    for( int i = 0; i < np; ++i ) {

        QTableWidgetItem    *ti;
        const ImProbeDat    &P = probes[i];

        // ---------
        // Row label
        // ---------

        if( !(ti = T->verticalHeaderItem( i )) ) {
            ti = new QTableWidgetItem;
            T->setVerticalHeaderItem( i, ti );
        }

        ti->setText( QString::number( i ) );

        // ----
        // Slot
        // ----

        if( !(ti = T->item( i, 0 )) ) {
            ti = new QTableWidgetItem;
            T->setItem( i, 0, ti );
            ti->setFlags( Qt::NoItemFlags );
        }

        ti->setText( QString::number( P.slot ) );

        // ----
        // Port
        // ----

        if( !(ti = T->item( i, 1 )) ) {
            ti = new QTableWidgetItem;
            T->setItem( i, 1, ti );
            ti->setFlags( Qt::NoItemFlags );
        }

        ti->setText( QString::number( P.port ) );

        // ----
        // HSSN
        // ----

        if( !(ti = T->item( i, 2 )) ) {
            ti = new QTableWidgetItem;
            T->setItem( i, 2, ti );
            ti->setFlags( Qt::NoItemFlags );
        }

        if( P.hssn == (quint32)-1 )
            ti->setText( "???" );
        else
            ti->setText( QString::number( P.hssn ) );

        // ----
        // HSFW
        // ----

        if( !(ti = T->item( i, 3 )) ) {
            ti = new QTableWidgetItem;
            T->setItem( i, 3, ti );
            ti->setFlags( Qt::NoItemFlags );
        }

        if( P.hsfw.isEmpty() )
            ti->setText( "???" );
        else
            ti->setText( P.hsfw );

        // --
        // SN
        // --

        if( !(ti = T->item( i, 4 )) ) {
            ti = new QTableWidgetItem;
            T->setItem( i, 4, ti );
            ti->setFlags( Qt::NoItemFlags );
        }

        if( P.sn == (quint64)std::numeric_limits<qlonglong>::max() )
            ti->setText( "???" );
        else
            ti->setText( QString::number( P.sn ) );

        // ----
        // Type
        // ----

        if( !(ti = T->item( i, 5 )) ) {
            ti = new QTableWidgetItem;
            T->setItem( i, 5, ti );
            ti->setFlags( Qt::NoItemFlags );
        }

        if( P.type == (quint16)-1 )
            ti->setText( "???" );
        else
            ti->setText( QString::number( P.type ) );

        // ----
        // Enab
        // ----

        if( !(ti = T->item( i, 6 )) ) {
            ti = new QTableWidgetItem;
            T->setItem( i, 6, ti );
        }

        if( comIdx == 0 )
            ti->setFlags( Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsUserCheckable );
        else
            ti->setFlags( Qt::NoItemFlags );

        ti->setCheckState( P.enab ? Qt::Checked : Qt::Unchecked );

        // --
        // Id
        // --

        if( !(ti = T->item( i, 7 )) ) {
            ti = new QTableWidgetItem;
            T->setItem( i, 7, ti );
            ti->setFlags( Qt::NoItemFlags );
        }

        if( P.ip == (quint16)-1 )
            ti->setText( "???" );
        else
            ti->setText( QString::number( P.ip ) );
    }
}


void CimCfg::ImProbeTable::fromGUI( QTableWidget *T )
{
    int np = T->rowCount();

    probes.resize( np );

    for( int i = 0; i < np; ++i ) {

        ImProbeDat          &P = probes[i];
        QTableWidgetItem    *ti;
        quint64             v64;
        uint                v32;
        int                 val;
        bool                ok;

        // ----
        // Slot
        // ----

        ti  = T->item( i, 0 );
        val = ti->text().toUInt( &ok );

        P.slot = (ok ? val : -1);

        // ----
        // Port
        // ----

        ti  = T->item( i, 1 );
        val = ti->text().toUInt( &ok );

        P.port = (ok ? val : -1);

        // ----
        // HSSN
        // ----

        ti  = T->item( i, 2 );
        v32 = ti->text().toUInt( &ok );

        P.hssn = (ok ? v32 : -1);

        // ----
        // HSFW
        // ----

        ti      = T->item( i, 3 );
        P.hsfw  = ti->text();

        if( P.hsfw.contains( "?" ) )
            P.hsfw.clear();

        // --
        // SN
        // --

        ti  = T->item( i, 4 );
        v64 = ti->text().toULongLong( &ok );

        P.sn = (ok ? v64 : (quint64)std::numeric_limits<qlonglong>::max());

        // ----
        // Type
        // ----

        ti  = T->item( i, 5 );
        val = ti->text().toUInt( &ok );

        P.type = (ok ? val : -1);

        // ----
        // Enab
        // ----

        ti  = T->item( i, 6 );

        P.enab = (ti->checkState() == Qt::Checked);

        // --
        // Id
        // --

        ti  = T->item( i, 7 );
        val = ti->text().toUInt( &ok );

        P.ip = (ok ? val : -1);
    }
}

/* ---------------------------------------------------------------- */
/* struct AttrEach ------------------------------------------------ */
/* ---------------------------------------------------------------- */

// Given input fields:
// - probe type parameter
//
// Derive:
// - imCumTypCnt[]
//
void CimCfg::AttrEach::deriveChanCounts( int type )
{
// --------------------------------
// First count each type separately
// --------------------------------

// MS: Analog and digital aux may be redefined in phase 3B2

    Q_UNUSED( type )

    imCumTypCnt[imTypeAP] = IMROTbl::imType0Chan;
    imCumTypCnt[imTypeLF] = imCumTypCnt[imTypeAP];
    imCumTypCnt[imTypeSY] = 1;

// ---------
// Integrate
// ---------

    for( int i = 1; i < imNTypes; ++i )
        imCumTypCnt[i] += imCumTypCnt[i - 1];
}


// Given input fields:
// - stdbyStr (trimmed)
// - nAP channels (parameter)
//
// Derive:
// - stdbyBits
//
// Return true if stdbyStr format OK.
//
bool CimCfg::AttrEach::deriveStdbyBits( QString &err, int nAP )
{
    err.clear();

    if( stdbyStr.isEmpty() )
        stdbyBits.fill( false, nAP );
    else if( Subset::isAllChansStr( stdbyStr ) ) {

        stdbyStr = "all";
        Subset::defaultBits( stdbyBits, nAP );
    }
    else if( Subset::rngStr2Bits( stdbyBits, stdbyStr ) ) {

        stdbyStr = Subset::bits2RngStr( stdbyBits );

        if( stdbyBits.size() > nAP ) {
            err = QString(
                    "Imec off-channel string includes channels"
                    " higher than maximum [%1].")
                    .arg( nAP - 1 );
            return false;
        }

        // in case smaller
        stdbyBits.resize( nAP );
    }
    else {
        err = "Imec off-channel string has incorrect format.";
        return false;
    }

    return true;
}


void CimCfg::AttrEach::justAPBits(
    QBitArray       &apBits,
    const QBitArray &saveBits ) const
{
    apBits = saveBits;
    apBits.fill( 0, imCumTypCnt[imTypeAP], imCumTypCnt[imTypeLF] );
}


void CimCfg::AttrEach::justLFBits(
    QBitArray       &lfBits,
    const QBitArray &saveBits ) const
{
    lfBits = saveBits;
    lfBits.fill( 0, 0, imCumTypCnt[imTypeAP] );
}


void CimCfg::AttrEach::apSaveBits( QBitArray &apBits ) const
{
    justAPBits( apBits, sns.saveBits );
}


void CimCfg::AttrEach::lfSaveBits( QBitArray &lfBits ) const
{
    justLFBits( lfBits, sns.saveBits );
}


int CimCfg::AttrEach::apSaveChanCount() const
{
    QBitArray   apBits;
    apSaveBits( apBits );
    return apBits.count( true );
}


int CimCfg::AttrEach::lfSaveChanCount() const
{
    QBitArray   lfBits;
    lfSaveBits( lfBits );
    return lfBits.count( true );
}


double CimCfg::AttrEach::chanGain( int ic ) const
{
    double  g = 1.0;

    if( ic > -1 ) {

        int nAP = imCumTypCnt[imTypeAP];

        if( ic < nAP )
            g = roTbl.e[ic].apgn;
        else if( ic < imCumTypCnt[imTypeLF] )
            g = roTbl.e[ic-nAP].lfgn;
        else
            return 1.0;

        if( g < 50.0 )
            g = 50.0;
    }

    return g;
}

/* ---------------------------------------------------------------- */
/* class CimCfg --------------------------------------------------- */
/* ---------------------------------------------------------------- */

int CimCfg::vToInt10( double v, int ip, int ic ) const
{
    return 1023 * all.range.voltsToUnity( v * each[ip].chanGain( ic ) ) - 512;
}


double CimCfg::int10ToV( int i10, int ip, int ic ) const
{
    return all.range.unityToVolts( (i10 + 512) / 1024.0 )
            / each[ip].chanGain( ic );
}


void CimCfg::loadSettings( QSettings &S )
{
// ---
// ALL
// ---

    S.beginGroup( "DAQ_Imec_All" );

//    all.range.rmin =
//    S.value( "imAiRangeMin", -0.6 ).toDouble();

//    all.range.rmax =
//    S.value( "imAiRangeMax", 0.6 ).toDouble();

//    all.srate =
//    S.value( "imSampRate", 30000.0 ).toDouble();

    all.trgSource =
    S.value( "imTrgSource", 0 ).toInt();

    all.trgRising =
    S.value( "imTrgRising", true ).toBool();

    nProbes =
    S.value( "imNProbes", 1 ).toInt();

    enabled =
    S.value( "imEnabled", false ).toBool();

    S.endGroup();

// ----
// Each
// ----

    S.beginGroup( "DAQ_Imec_Each" );

    each.resize( nProbes > 0 ? nProbes : 1 );

    for( int ip = 0; ip < nProbes; ++ip ) {

        S.beginGroup( QString("Probe%1").arg( ip ) );

        AttrEach    &E = each[ip];

        E.imroFile  = S.value( "imRoFile", QString() ).toString();
        E.stdbyStr  = S.value( "imStdby", QString() ).toString();
        E.skipCal   = S.value( "imSkipCal", false ).toBool();
        E.LEDEnable = S.value( "imLEDEnable", false ).toBool();

        E.sns.shankMapFile =
        S.value( "imSnsShankMapFile", QString() ).toString();

        E.sns.chanMapFile =
        S.value( "imSnsChanMapFile", QString() ).toString();

        E.sns.uiSaveChanStr =
        S.value( "ImSnsSaveChanSubset", "all" ).toString();

        S.endGroup();
    }

    S.endGroup();
}


void CimCfg::saveSettings( QSettings &S ) const
{
// ---
// ALL
// ---

    S.beginGroup( "DAQ_Imec_All" );

    S.setValue( "imAiRangeMin", all.range.rmin );
    S.setValue( "imAiRangeMax", all.range.rmax );
    S.setValue( "imSampRate", all.srate );
    S.setValue( "imTrgSource", all.trgSource );
    S.setValue( "imTrgRising", all.trgRising );
    S.setValue( "imNProbes", nProbes );
    S.setValue( "imEnabled", enabled );

    S.endGroup();

// ----
// Each
// ----

    S.remove( "DAQ_Imec_Each" );
    S.beginGroup( "DAQ_Imec_Each" );

    for( int ip = 0; ip < nProbes; ++ip ) {

        S.beginGroup( QString("Probe%1").arg( ip ) );

        const AttrEach  &E = each[ip];

        S.setValue( "imRoFile", E.imroFile );
        S.setValue( "imStdby", E.stdbyStr );
        S.setValue( "imSkipCal", E.skipCal );
        S.setValue( "imLEDEnable", E.LEDEnable );
        S.setValue( "imSnsShankMapFile", E.sns.shankMapFile );
        S.setValue( "imSnsChanMapFile", E.sns.chanMapFile );
        S.setValue( "imSnsSaveChanSubset", E.sns.uiSaveChanStr );

        S.endGroup();
    }

    S.endGroup();
}


bool CimCfg::detect( QStringList &sl, ImProbeTable &T )
{
    bool    ok = false;

    T.init();
    sl.clear();

// ------------------
// Local reverse maps
// ------------------

    QMap<int,int>   mapSlots;   // ordered keys, arbitrary value
    QVector<int>    loc_id2dat;
    QVector<int>    loc_slot;

    for( int i = 0, n = T.probes.size(); i < n; ++i ) {

        CimCfg::ImProbeDat  &P = T.probes[i];

        if( P.enab ) {

            loc_id2dat.push_back( i );
            mapSlots[P.slot] = 0;
        }
    }

    QMap<int,int>::iterator it;

    for( it = mapSlots.begin(); it != mapSlots.end(); ++it )
        loc_slot.push_back( it.key() );

// ----------
// Local vars
// ----------

#ifdef HAVE_IMEC
    NeuropixAPI     IM;
    QString         addr;
    int             err;
    qint32          i32;
    quint8          maj8, min8;
#endif

// ------
// OpenBS
// ------

#ifdef HAVE_IMEC
    if( T.comIdx == 0 ) {
        sl.append( "PXI interface not yet supported." );
        return false;
    }

    addr = QString("10.2.0.%1").arg( T.comIdx - 1 );

    err = IM.openBS( STR2CHR( addr ) );

    if( err != SUCCESS ) {
        sl.append(
            QString("IMEC openBS( %1 ) error %2.")
            .arg( addr ).arg( err ) );
        sl.append( "Check connections and power." );
        sl.append( "Set IP address 10.1.1.1." );
        sl.append( "Gateway 255.0.0.0." );
        return false;
    }
#endif

// -------
// APIVers
// -------

#ifdef HAVE_IMEC
    err = IM.getAPIVersion( maj8, min8 );

    if( err != SUCCESS ) {
        sl.append( QString("IMEC getAPIVersion error %1.").arg( err ) );
        goto exit;
    }

    T.api = QString("%1.%2").arg( maj8 ).arg( min8 );
#else
    T.api = "0.0";
#endif

    sl.append( QString("API version %1").arg( T.api ) );

// ---------------
// Loop over slots
// ---------------

    for( int is = 0, ns = loc_slot.size(); is < ns; ++is ) {

        ImSlotVers  V;
        int         slot = loc_slot[is];

        // ----
        // BSFW
        // ----

#ifdef HAVE_IMEC
        err = IM.getBSBootVersion( slot, maj8, min8 );

        if( err != SUCCESS ) {
            sl.append(
                QString("IMEC getBSBootVersion(slot %1) error %1.")
                .arg( slot ).arg( err ) );
            goto exit;
        }

        V.bsfw = QString("%1.%2").arg( maj8 ).arg( min8 );
#else
        V.bsfw = "0.0";
#endif

        sl.append(
            QString("BS(slot %1) firmware version %2")
            .arg( slot ).arg( V.bsfw ) );

        // -----
        // BSCSN
        // -----

#ifdef HAVE_IMEC
        err = IM.readBSCSN( slot, i32 );

        if( err != SUCCESS ) {
            sl.append(
                QString("IMEC readBSCSN(slot %1) error %1.")
                .arg( slot ).arg( err ) );
            goto exit;
        }

        V.bscsn = QString::number( i32 );
#else
        V.bscsn = "0";
#endif

        sl.append(
            QString("BSC(slot %1) serial number %2")
            .arg( slot ).arg( V.bscsn ) );

        // -----
        // BSCHW
        // -----

#ifdef HAVE_IMEC
        err = IM.getBSCVersion( slot, maj8, min8 );

        if( err != SUCCESS ) {
            sl.append(
                QString("IMEC getBSCVersion(slot %1) error %1.")
                .arg( slot ).arg( err ) );
            goto exit;
        }

        V.bschw = QString("%1.%2").arg( maj8 ).arg( min8 );
#else
        V.bschw = "0.0";
#endif

        sl.append(
            QString("BSC(slot %1) hardware version %2")
            .arg( slot ).arg( V.bschw ) );

        // -----
        // BSCFW
        // -----

// MS: Temporarily disabled this query which errors and hangs
#ifdef HAVE_IMECX
        err = IM.getBSCBootVersion( slot, maj8, min8 );

        if( err != SUCCESS ) {
            sl.append(
                QString("IMEC getBSCBootVersion(slot %1) error %1.")
                .arg( slot ).arg( err ) );
            goto exit;
        }

        V.bscfw = QString("%1.%2").arg( maj8 ).arg( min8 );
#else
        V.bscfw = "0.0";
#endif

        sl.append(
            QString("BSC(slot %1) firmware version %2")
            .arg( slot ).arg( V.bscfw ) );

        // -------------
        // Add map entry
        // -------------

        T.slot2Vers[slot] = V;
    }

// --------------------
// Individual HS/probes
// --------------------

    for( int ip = 0, np = loc_id2dat.size(); ip < np; ++ip ) {

        ImProbeDat  &P = T.probes[loc_id2dat[ip]];

        // --------------------
        // Connect to that port
        // --------------------

#ifdef HAVE_IMEC
        err = IM.openProbe( P.slot, P.port );

        if( err != SUCCESS ) {
            sl.append(
                QString("IMEC openProbe(slot %1, port %2) error %3.")
                .arg( P.slot ).arg( P.port ).arg( err ) );
            goto exit;
        }
#endif

        // ----
        // HSSN
        // ----

#ifdef HAVE_IMEC
        err = IM.readHSSN( P.slot, P.port, i32 );

        if( err != SUCCESS ) {
            sl.append(
                QString("IMEC readHSSN(slot %1, port %2) error %3.")
                .arg( P.slot ).arg( P.port ).arg( err ) );
            goto exit;
        }

        P.hssn = i32;
#else
        P.hssn = 0;
#endif

        // ----
        // HSFW
        // ----

#ifdef HAVE_IMEC
        err = IM.getHSVersion( P.slot, P.port, maj8, min8 );

        if( err != SUCCESS ) {
            sl.append(
                QString("IMEC getHSVersion(slot %1, port %2) error %3.")
                .arg( P.slot ).arg( P.port ).arg( err ) );
            goto exit;
        }

        P.hsfw = QString("%1.%2").arg( maj8 ).arg( min8 );;
#else
        P.hsfw = "0.0";
#endif

        // --
        // SN
        // --

#ifdef HAVE_IMEC
        quint64 i64;

        err = IM.readId( P.slot, P.port, i64 );

        if( err != SUCCESS ) {
            sl.append(
                QString("IMEC readId(slot %1, port %2) error %3.")
                .arg( P.slot ).arg( P.port ).arg( err ) );
            goto exit;
        }

        P.sn = i64;
#else
        P.sn = 0;
#endif
    }

// ----
// Exit
// ----

    ok = true;

#ifdef HAVE_IMEC
exit:
    for( int is = 0, ns = loc_slot.size(); is < ns; ++is )
        IM.close( loc_slot[is], -1 );
#endif

    return ok;
}


