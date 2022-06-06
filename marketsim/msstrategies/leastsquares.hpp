#include<vector>
#include<numeric>

inline std::vector<double> getRegressCoef(std::vector<double> ins, std::vector<double> outs, bool intercept = false)
{
	double xavg = std::accumulate(ins.begin(), ins.end(), 0.0) / ins.size();
	double yavg = std::accumulate(outs.begin(), outs.end(), 0.0) / outs.size();

	int n = ins.size();
	double xy = 0.0, xx = 0.0,
		c = intercept ? n * xavg * yavg : 0, d = intercept ? n * xavg * xavg : 0;
	for (int i = 0; i < n; i++)
	{
		xy += ins[i] * outs[i];
		xx += ins[i] * ins[i];
	}
	double beta = (xy - c) / (xx - d);

	return std::vector<double>{ intercept * (yavg - beta) * xavg, beta };
}