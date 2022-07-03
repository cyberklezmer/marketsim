#ifndef HEURISTICSTRATEGY_HPP
#define HEURISTICSTRATEGY_HPP

#include "marketsim.hpp"

namespace marketsim
{



struct theuristicstrategysetting
{
    // weight of risk when deciding
    double lambda = 0.5;
    // CVaR level (CVaR determines the risk part)
    double alpha = 0.05;
    // interval of reentry before it starts to be determined by volume
    tabstime initinterval = 1;
    // time over which the volumes are averaged when estimating future volume
    double qaverageinterval = 100;
    // the strategy tries to se up the time interval so that the overall buy and sell
    // is equal to theis value
    tvolume desiredmean = 13;
    // see mm.lyx
    tprice Delta = 1;
    // max volume cosidered
    tvolume maxvol = 10;
    // step with whhich the volues are considered
    unsigned voldelta = 2;
    // maximal consumption
    tprice maxconsumption = 50;
    // determines how much the strategy keeps until it consumes
    double safetyfactor = 1;
};

class heuristicstrategy : public teventdrivenstrategy
{
public:

    struct args
    {
        tprice a;
        tprice b;
        tprice c;
        tvolume v;
        tvolume w;
        double V = std::numeric_limits<double>::quiet_NaN();
        bool isnan() const { return std::isnan(V); }
    };


    heuristicstrategy(const theuristicstrategysetting& s=
           theuristicstrategysetting()) :
        teventdrivenstrategy(s.initinterval),
        fsetting(s)
    {}

    virtual trequest event(const tmarketinfo& mi, tabstime tnow, trequestresult*)
    {
        // check for the last kown price
        double ldp = mi.lastdefinedp();
        // check for alpha and beta
        tprice alpha = mi.alpha();
        tprice beta = mi.beta();
        // if the others do not quote, estimate alpha and beta by own recent quotes
        if(alpha == khundefprice)
            alpha = lasta;
        if(beta == klundefprice)
            beta = lastb;
        // if this failed, then look for last defined a and b
        if(alpha == khundefprice)
            alpha = mi.lastdefineda();
        if(beta == klundefprice)
            beta = mi.lastdefinedb();
        // now it is inprobable that we did not set alpha nad beta
        // yet we check
        if(alpha != khundefprice && beta != khundefprice)
        {
            // it may also happen that alpha and beta are crossed
            if(alpha <= beta)
            {
                if(uniform()<0.5)
                    alpha = beta+1;
                else if(alpha > 1)
                    beta = alpha - 1;
                else
                {
                    beta = 1;
                    alpha = 2;
                }
            }

            assert(alpha != khundefprice);
            assert(beta != klundefprice);

            // interval over which we will averate
            double aint = std::min(tnow,fsetting.qaverageinterval);

            // sum the trades over the interval
            tvolume sumq = mi.history().sumq(tnow-aint,tnow);
            // ... and estimate the intensity of buying (assumed the same as for selling)
            double intensity = std::max(1.0, sumq / aint / 2.0)*interval();

            assert(intensity > 0);

            // now look how much we have
            tprice m = mi.mywallet().money();
            tvolume n = mi.mywallet().stocks();

            // some logging (has to be enabled in def
            std::ostringstream s;
            s << "t=" << tnow << " m=" << m << " n=" << n
              << " beta=" << beta << " alpha=" << alpha
              << " intensity=" << intensity
              << " Bvol=" << mi.orderbook().B.volume()
                 << ", Avol=" << mi.orderbook().A.volume();
            possiblylog("Calling evaluateV:",s.str());

            // now find "optimal" a,b,c,v,w
            args opt=optimize(mi,m,n,alpha,beta,intensity);

            // this should not happen, but to be sure....
            if(opt.isnan())
                return trequest();
            else
            {
                // logging again
                std::ostringstream s;
                s <<  "c=" << opt.c << " a=" << opt.a
                   << " b=" << opt.b  << " v=" << opt.v  << " w=" << opt.w;

                possiblylog("about to apply", s.str());

                // if we have enough observation
                if(tnow > fsetting.qaverageinterval)
                {
                    // set the time so that the buy (and sell) volume
                    // during this interval is averagly equal to fsetting.desiredmean
                    auto numpersec = sumq / fsetting.qaverageinterval;
                    setinterval(2.0 * fsetting.desiredmean / numpersec);
                }


                // save the history
                lasta = opt.a;
                lastb = opt.b;
                lastv = opt.v;
                lastw = opt.w;

                tpreorderprofile pp;

                // set the quotes
                pp.A.add(tpreorder(opt.a,opt.v));
                pp.B.add(tpreorder(opt.b,opt.w));
                return {pp,trequest::teraserequest(true),opt.c};
             }
        }
        // if for some reason we could not set quotes, we do nothing
        return trequest();
    }

private:

    args optimize(const tmarketinfo& info, tprice m, tvolume n, tprice alpha, tprice beta, double lambda)
    {
       args opt;
       // go through all possible a,b,...
       for(tprice a=std::max(alpha-1,beta+1); a <= alpha+fsetting.Delta; a++)
            for(tprice b=std::max(beta-fsetting.Delta,1); b<= beta+1; b++)
                if(a > b)
                {
                    // compute pi_b - the volume in queue in front of my b
                    tvolume pib = 0;
                    auto B = info.orderbook().B;

                    for(unsigned i=0; i<B.size(); i++)
                    {
                        auto o = B[i];
                        if(o.price < b)
                            break;
                        bool my = o.owner == info.myindex();
                        if(my)
                        {
                            // my existing order has the same price, so it
                            // will not e repleaced and put to the end of
                            // queue of the orders with the same price
                            if(o.price==b)
                                break; // we neglect the fact that the new volume could be higher
                                       // so the excess volume would stand at the end of the qeueue
                            else
                                continue;  // this order will be withdrawn
                        }
                        pib += o.volume;
                    }

                    // the same with sell orders
                    tvolume pia = 0;
                    auto A = info.orderbook().A;

                    for(unsigned i=0; i<A.size(); i++)
                    {
                        auto o = A[i];
                        if(o.price > a)
                            break;
                        bool my = o.owner == info.myindex();
                        if(my)
                        {
                            if(o.price==a)
                                break; // we neglect the fact that the new volume could be higher
                                       // so the excess volume will stand at the end of the qeueue
                            else
                                continue;  // this order will be withdrawn
                        }
                        pia += o.volume;
                    }
                    tvolume purchasablevol = m / b;
                    // now go thorugh all the possible valumes
                    for(tvolume v = 0;
                        v <= std::min(fsetting.maxvol,n);
                        v += fsetting.voldelta)
                        for(tvolume w = 0;
                            w <= std::min(fsetting.maxvol,purchasablevol);
                            w += fsetting.voldelta)
                        {
                            // if we are over the safety threshold, we consume
                            tprice c = std::max(0,
                                        std::min(fsetting.maxconsumption,
                                          m - w*b - static_cast<tprice>(fsetting.safetyfactor * fsetting.maxvol * alpha))) ;

                            assert(c>=0);
                            // now we comupte the distribution of C (actual sells)
                            double p = exp(-lambda);
                            double sump = p;
                            std::vector<double> LC(1,0);
                            for(int i=0;;)
                            {
                                if(i<=pia)
                                    LC[0] += p;
                                else
                                    LC.push_back(p);

                                if(i==pia+v)
                                {
                                    LC[v] += 1-sump;
                                    break;
                                }
                                i++;
                                p *= lambda / i;
                                sump += p;
                            }

                            // the same for D
                            p = exp(-lambda);
                            sump = p;
                            std::vector<double> LD(1,0);
                            for(int i=0;;)
                            {
                                if(i<=pib)
                                    LD[0] += p;
                                else
                                    LD.push_back(p);

                                if(i==pib+w)
                                {
                                    LD[w] += 1-sump;
                                    break;
                                }
                                i++;
                                p *= lambda / i;
                                sump += p;
                            }


                            // and now the distribution of our possible earinigs
                            finitedistribution<double> dist;

                            for(tvolume C=0; C< LC.size(); C++)
                                for(tvolume D=0; D< LD.size(); D++)
                                {
                                    double prob = LC[C] * LD[D];
                                    double v = -D*b+C*a + (D-C) *(alpha+beta)/2.0;
                                    dist.add(prob,v);
                                }


                            assert(dist.check());
                            // here we compute mean-CVaR out of it
                            // (but if lambda in settings is 0 this is nothing else
                            // but the expected value
                            double thisV =  dist.meanCVaR(fsetting.lambda,fsetting.alpha);

                            // look if we are better than before...
                            if(std::isnan(opt.V) || thisV > opt.V)
                            {
                                opt.a = a;
                                opt.b = b;
                                opt.c = c;
                                opt.v = v;
                                opt.w = w;
                                opt.V = thisV;
                                //std::clog << "a=" << a << " pia=" << pia << ", v=" << v <<  ", EC=" << E(LC) << " w=" << w << " b=" << b << " pib=" << pib << ", ED=" << E(LD) << " crit=" << EV << std::endl;

                            }
                        }

                }
       assert(!opt.isnan());
       return opt;
   }
    tprice lasta = khundefprice;
    tprice lastb = klundefprice;

    tvolume lastv = 0;
    tvolume lastw = 0;

//    bool finitialized;
    theuristicstrategysetting fsetting;
};

} // namespace

#endif // HEURISTICSTRATEGY_HPP
