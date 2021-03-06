#include "DistributionInterface.h"


GammaGenerator::GammaGenerator() {
	generator_ = std::make_unique<std::mt19937_64>();
}

double GammaGenerator::getGamma() const {
	return gamma_(*generator_);
}

