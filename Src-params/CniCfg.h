#ifndef CNICFG_H
#define CNICFG_H

#include "SGLTypes.h"
#include "SnsMaps.h"

#include <QMultiMap>

class QSettings;

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

// Base class for NI-DAQ configuration
//
class CniCfg
{
    // ---------
    // Constants
    // ---------

public:
    enum TermConfig {
        Default     = -1,
        RSE         = 10083,
        NRSE        = 10078,
        Diff        = 10106,
        PseudoDiff  = 12529
    };

    enum niTypeId {
        niTypeMN    = 0,
        niTypeMA    = 1,
        niTypeXA    = 2,
        niTypeXD    = 3,
        niSumNeural = 0,
        niSumAnalog = 2,
        niSumAll    = 3,
        niNTypes    = 4
    };

    // -----
    // Types
    // -----

    // ------
    // Params
    // ------

    // derived:
    // niCumTypCnt[]

private:
    // These depend upon isDualDevMode so use accessors.
    QString         _uiMNStr2,
                    _uiMAStr2,
                    _uiXAStr2,
                    _uiXDStr2;

public:
    VRange          range;
    double          srate,
                    mnGain,
                    maGain;
    QString         dev1,
                    dev2,
                    clockStr1,
                    clockStr2,
                    uiMNStr1,
                    uiMAStr1,
                    uiXAStr1,
                    uiXDStr1,
                    syncLine;
    int             niCumTypCnt[niNTypes];
    uint            muxFactor;
    TermConfig      termCfg;
    bool            enabled,
                    isDualDevMode,
                    syncEnable;
    SnsChansNidq    sns;

    // -------------
    // Param methods
    // -------------

public:
    void setUIMNStr2( const QString &s )    {_uiMNStr2=s;}
    void setUIMAStr2( const QString &s )    {_uiMAStr2=s;}
    void setUIXAStr2( const QString &s )    {_uiXAStr2=s;}
    void setUIXDStr2( const QString &s )    {_uiXDStr2=s;}
    QString uiMNStr2() const                {return (isDualDevMode ? _uiMNStr2 : "");}
    QString uiMAStr2() const                {return (isDualDevMode ? _uiMAStr2 : "");}
    QString uiXAStr2() const                {return (isDualDevMode ? _uiXAStr2 : "");}
    QString uiXDStr2() const                {return (isDualDevMode ? _uiXDStr2 : "");}
    QString uiMNStr2Bare() const            {return _uiMNStr2;}
    QString uiMAStr2Bare() const            {return _uiMAStr2;}
    QString uiXAStr2Bare() const            {return _uiXAStr2;}
    QString uiXDStr2Bare() const            {return _uiXDStr2;}

    bool isMuxingMode() const
        {return !uiMNStr1.isEmpty()
                || !uiMAStr1.isEmpty()
                || !uiMNStr2().isEmpty()
                || !uiMAStr2().isEmpty();}

    bool isClock1Internal() const   {return clockStr1 == "Internal";}
    double chanGain( int ic ) const;

    void deriveChanCounts();

    int vToInt16( double v, int ic ) const;
    double int16ToV( int i16, int ic ) const;

    void loadSettings( QSettings &S );
    void saveSettings( QSettings &S ) const;

// -------
// Statics
// -------

public:

    // -----
    // Types
    // -----

    typedef QMultiMap<QString,VRange>   DeviceRangeMap;
    typedef QMap<QString,QStringList>   DeviceChanMap;
    typedef QMap<QString,int>           DeviceChanCount;

    // ----
    // Data
    // ----

    static DeviceRangeMap   aiDevRanges,
                            aoDevRanges;
    static DeviceChanCount  aiDevChanCount,
                            aoDevChanCount,
                            diDevLineCount;

    // -------
    // Methods
    // -------

    static TermConfig stringToTermConfig( const QString &txt );
    static QString termConfigToString( TermConfig t );

    static QStringList getPFIChans( const QString &dev );

    static QStringList getAllDOLines();

    static QString parseDIStr(
        QString         &dev,
        int             &line,
        const QString   &lineStr );

    static bool isHardware();
    static void probeAIHardware();
    static void probeAOHardware();
    static void probeAllDILines();

    static bool supportsAISimultaneousSampling( const QString &dev );

    static double maxSampleRate( const QString &dev, int nChans = 1 );
    static double minSampleRate( const QString &dev );

    static QString getProductName( const QString &dev );

    static QString setDO( const QString &line, bool onoff );

    static double sampleFreq(
        const QString   &dev,
        const QString   &pfi,
        const QString   &line );
};

#endif  // CNICFG_H


