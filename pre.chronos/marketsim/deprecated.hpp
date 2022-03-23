#ifndef DEPRECATED_HPP
#define DEPRECATED_HPP

#include "marketsim.hpp"

namespace marketsim
{

class mmstrategy: public tstrategy
{
public:
    mmstrategy(const std::string& name)
     : tstrategy(name)
    {}


private:
    virtual trequest event(const tmarketinfo& mi,
                           const ttradinghistory& )
    {
        std::vector<bool> ae;
        std::vector<bool> be;

        trequest::teraserequest er(false);
        tpreorderprofile rp;

        assert(mi.myprofile.A.size()<=1);
        assert(mi.myprofile.B.size()<=1);

        tprice othersa = kinfprice;
        for(unsigned i=0; i<mi.orderbook.A.size(); i++)
        {
            auto r = mi.orderbook.A[i];
            if(r.owner != mi.myid)
            {
                othersa = r.price;
                break;
            }
        }

        tprice othersb = kminfprice;

        for(unsigned i=0; i<mi.orderbook.B.size(); i++)
        {
            auto r = mi.orderbook.B[i];
            if(r.owner != mi.myid)
            {
                othersb = r.price;
                break;
            }
        }

        tprice myolda = mi.myprofile.a();
        tprice myoldb = mi.myprofile.b();

        if(othersa != kinfprice && othersb != kminfprice)
        {
            tprice mynewa = othersa-1 > othersb ? othersa - 1 : othersa;
            tprice mynewb = othersb+1 < othersa ? othersb + 1 : othersb;
            if(mynewa == mynewb)
            {
                if(orpp::sys::uniform() > 0.5)
                    mynewa++;
                else
                    mynewb--;
            }
            if(mynewa != myolda)
            {
                if(myolda != kinfprice)
                    er.a.push_back(true);
                rp.A.add({mynewa,1});
            }
            if(mynewb != myoldb)
            {
                if(myoldb != kminfprice)
                    er.b.push_back(true);
                rp.B.add({mynewb,1});
            }

        }

        return trequest(rp,er,0);
    }


    virtual void atend(const ttradinghistory&)
    {

    }

    virtual void warning(trequest::ewarning, const std::string&)
    {
    }

};



}

#endif // DEPRECATED_HPP
