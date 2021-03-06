#ifndef CIMCFG_H
#define CIMCFG_H

#include "SGLTypes.h"
#include "SnsMaps.h"

#include <QMap>

#include <limits>

class QSettings;
class QTableWidget;

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

// MS: Does stdby still make sense?

struct IMRODesc
{
    qint16  bank,
            apgn,   // gain, not index
            lfgn;   // gain, not index
    qint8   refid,  // reference index
            apflt;  // bool

    IMRODesc()
    :   bank(0), apgn(500), lfgn(250), refid(0), apflt(1)               {}
    IMRODesc( int bank, int refid, int apgn, int lfgn, bool apflt )
    :   bank(bank), apgn(apgn), lfgn(lfgn), refid(refid), apflt(apflt)  {}
    bool operator==( const IMRODesc &rhs ) const
        {return bank==rhs.bank && apgn==rhs.apgn && lfgn==rhs.lfgn
            && refid==rhs.refid && apflt==rhs.apflt;}
    QString toString( int chn ) const;
    static IMRODesc fromString( const QString &s );
};


struct IMROTbl
{
    enum imLims {
        imType0Elec     = 960,

        imType0Banks    = 3,

        imType0Chan     = 384,

        imNRefids       = 5,

        imNGains        = 8
    };

    quint32             type;
    QVector<IMRODesc>   e;

    void fillDefault( int type );

    int nChan() const   {return e.size();}
    int nElec() const   {return typeToNElec( type );}

    bool operator==( const IMROTbl &rhs ) const
        {return type==rhs.type && e == rhs.e;}
    bool operator!=( const IMROTbl &rhs ) const
        {return !(*this == rhs);}

    bool banksSame( const IMROTbl &rhs ) const;

    QString toString() const;
    void fromString( const QString &s );

    bool loadFile( QString &msg, const QString &path );
    bool saveFile( QString &msg, const QString &path );

    static int typeToNElec( int type );
    static int chToEl384( int ch, int bank );
    static bool chIsRef( int ch );
    static int idxToGain( int idx );
    static int gainToIdx( int gain );
};


// Base class for IMEC configuration
//
class CimCfg
{
    // ---------
    // Constants
    // ---------

public:
    enum imTypeId {
        imTypeAP    = 0,
        imTypeLF    = 1,
        imTypeSY    = 2,
        imSumAP     = 0,
        imSumNeural = 1,
        imSumAll    = 2,
        imNTypes    = 3
    };

    // -----
    // Types
    // -----

    struct ImProbeDat {
        quint16     slot,       // ini
                    port;       // ini
        quint32     hssn;       // detect
        QString     hsfw;       // detect
        quint64     sn;         // detect
        quint16     type;       // detect
        bool        enab;       // ini
        quint16     ip;         // calc

        ImProbeDat( int slot, int port )
        :   slot(slot), port(port), enab(true)  {init();}
        ImProbeDat()
        :   enab(true)                          {init();}

        void init()
            {
                hssn    = -1;
                hsfw.clear();
                sn      = std::numeric_limits<qlonglong>::max();
                type    = -1;
                ip      = -1;
            }

        bool operator< ( const ImProbeDat &rhs ) const
            {
                if( slot < rhs.slot )
                    return true;
                else if( slot == rhs.slot )
                    return port < rhs.port;
                else
                    return false;
            }

        void load( QSettings &S, int i );
        void save( QSettings &S, int i ) const;
    };

    struct ImSlotVers {
        QString     bsfw,   // maj.min
                    bscsn,
                    bschw,  // maj.min
                    bscfw;  // maj.min
    };

    struct ImProbeTable {
        int                     comIdx;     // 0=PXI
        QVector<ImProbeDat>     probes;
        QVector<int>            id2dat;     // probeID -> ImProbeDat
        QVector<int>            slot;       // used slots
        QString                 api;        // maj.min
        QMap<int,ImSlotVers>    slot2Vers;

        void init();

        void loadSettings();
        void saveSettings() const;

        void toGUI( QTableWidget *T ) const;
        void fromGUI( QTableWidget *T );
    };

    // -------------------------------
    // Attributes common to all probes
    // -------------------------------

    struct AttrAll {
        VRange  range;
        double  srate;
        int     trgSource;
        bool    trgRising;

        AttrAll() : range(VRange(-0.6,0.6)), srate(3e4)  {}
    };

    // --------------------------
    // Attributes for given probe
    // --------------------------

    struct AttrEach {
        QString         imroFile,
                        stdbyStr;
        IMROTbl         roTbl;
        QBitArray       stdbyBits;
        int             imCumTypCnt[imNTypes];
        bool            skipCal,
                        LEDEnable;
        SnsChansImec    sns;

        AttrEach() : skipCal(false), LEDEnable(false)   {}

        void deriveChanCounts( int type );
        bool deriveStdbyBits( QString &err, int nAP );

        void justAPBits(
            QBitArray       &apBits,
            const QBitArray &saveBits ) const;

        void justLFBits(
            QBitArray       &lfBits,
            const QBitArray &saveBits ) const;

        void apSaveBits( QBitArray &apBits ) const;
        void lfSaveBits( QBitArray &lfBits ) const;

        int apSaveChanCount() const;
        int lfSaveChanCount() const;

        double chanGain( int ic ) const;
    };

    // ------
    // Params
    // ------

    // derived:
    // stdbyBits
    // each.imCumTypCnt[]

public:
    AttrAll             all;
    QVector<AttrEach>   each;
    int                 nProbes;
    bool                enabled;

    CimCfg() : nProbes(1), enabled(false)   {each.resize( 1 );}

    // -------------
    // Param methods
    // -------------

public:
    int vToInt10( double v, int ip, int ic ) const;
    double int10ToV( int i10, int ip, int ic ) const;

    void loadSettings( QSettings &S );
    void saveSettings( QSettings &S ) const;

    // ------
    // Config
    // ------

    static bool detect( QStringList &sl, ImProbeTable &T );
};

#endif  // CIMCFG_H


