#ifndef GRAPHFETCHER_H
#define GRAPHFETCHER_H

#include <QObject>
#include <QMutex>
#include <QVector>

class SVGrafsM;
class AIQ;

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

struct GFStream {
    QString     stream;
    SVGrafsM    *W;
    AIQ         *aiQ;
    quint64     nextCt;

    GFStream()
        :   W(0), aiQ(0), nextCt(0)                 {}
    GFStream( const QString &stream, SVGrafsM *W )
        :   stream(stream), W(W), aiQ(0), nextCt(0) {}
};

class GFWorker : public QObject
{
    Q_OBJECT

private:
    QVector<GFStream>   gfs;
    mutable QMutex      gfsMtx,
                        runMtx;
    volatile bool       hardPaused, // Pause button
                        softPaused, // Window state
                        pleaseStop;

public:
    GFWorker()
    :   QObject(0),
        hardPaused(false), softPaused(false),
        pleaseStop(false)                   {}
    virtual ~GFWorker()                     {}

    void setStreams( const QVector<GFStream> &gfs )
        {QMutexLocker ml( &gfsMtx ); this->gfs = gfs;}

    void hardPause( bool pause )
        {QMutexLocker ml( &runMtx ); hardPaused = pause;}
    void softPause( bool pause )
        {QMutexLocker ml( &runMtx ); softPaused = pause;}
    bool isPaused() const
        {QMutexLocker ml( &runMtx ); return hardPaused || softPaused;}

    void stop()             {QMutexLocker ml( &runMtx ); pleaseStop = true;}
    bool isStopped() const  {QMutexLocker ml( &runMtx ); return pleaseStop;}

signals:
    void finished();

public slots:
    void run();

private:
    void fetch( GFStream &S, double loopT, double oldestSecs );
};


class GraphFetcher
{
private:
    QThread     *thread;
    GFWorker    *worker;

public:
    GraphFetcher();
    virtual ~GraphFetcher();

    void setStreams( const QVector<GFStream> &gfs )
        {worker->setStreams( gfs );}

    void hardPause( bool pause )    {worker->hardPause( pause );}
    void softPause( bool pause )    {worker->softPause( pause );}
};

#endif  // GRAPHFETCHER_H


