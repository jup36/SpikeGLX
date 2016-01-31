#ifndef TRIGIMMED_H
#define TRIGIMMED_H

#include "TrigBase.h"

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class TrigImmed : public TrigBase
{
public:
    TrigImmed(
        DAQ::Params     &p,
        GraphsWindow    *gw,
        const AIQ       *imQ,
        const AIQ       *niQ )
    : TrigBase( p, gw, imQ, niQ ) {}

    virtual void setGate( bool hi );
    virtual void resetGTCounters();

public slots:
    virtual void run();

private:
    bool bothWriteSome(
        int     &ig,
        int     &it,
        quint64 &imNextCt,
        quint64 &niNextCt );

    bool eachWriteSome(
        DataFile    *df,
        const AIQ   *aiQ,
        quint64     &nextCt );
};

#endif  // TRIGIMMED_H


