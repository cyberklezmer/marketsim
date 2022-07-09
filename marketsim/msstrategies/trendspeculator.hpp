#include "marketsim.hpp"

namespace marketsim
{

	///  A speculative strategy using optimizing via trend movement

	class trendspeculator : public teventdrivenstrategy
	{

	public:
		trendspeculator() : teventdrivenstrategy(1)
		{
		}