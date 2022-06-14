#include<vector>
#include<numeric>

using Tvec = std::vector<double>;
using T2vec = std::vector<Tvec>;

Tvec getRegressCoef(Tvec ins, Tvec outs)
{
	double xavg = std::accumulate(ins.begin(), ins.end(), 0.0) / ins.size();
	double yavg = std::accumulate(outs.begin(), outs.end(), 0.0) / outs.size();

	int n = ins.size();
	double beta = 0, c = n * xavg * yavg, d = n * xavg * xavg;
	for (int i = 0; i < n; i++)
		beta += (ins[i] * outs[i] - c) / (ins[i] * ins[i] - d);

	return Tvec({ yavg - beta * xavg, beta });
}