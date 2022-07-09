#include "marketsim.hpp"

namespace marketsim
{

	///  A speculative strategy optimizing depending on trend movement

	class trendspeculator : public teventdrivenstrategy
	{

	public:
		trendspeculator() : teventdrivenstrategy(1)
		{
		}
	};

}