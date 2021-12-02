#pragma once
#include <cmath>
#include <array>
#include <algorithm>
#include <iterator>
#include <exception>
#include "ExpDistribution.h"
#include "ComptonDistribution.h"
#include "GaussianDistribution.h"

namespace units {
	const double cm = 1.0;
	const double eMass = 1.0;
}


struct Mu {
	double photoEffect;
	double comptonEffect;
	double pairProduction;
};


class Photon {
public:
	bool isAlive_;
	const bool isDirectedForward_;
	double energy_;
	double z_;

	void move(double dz) {
		z_ += dz * (isDirectedForward_ ? 1.0 : -1.0);
	}
};


class Geant5 {
public: 
	using energy_t = double;
	using length_t = double;
private:
	const energy_t initialEnergy_;
	const length_t zMax_;
	
	// reusable distributions
	ExpDistribution expDist_; 
	ComptonDistribution comptonDist_;
	GaussianDistribution gaussDist_;

public:
	Geant5(const energy_t initialEnergy = 8.0 * units::eMass, length_t zMax = 3.0 * units::cm)
		: initialEnergy_{ initialEnergy }, zMax_{ zMax } {}

	// single run that returns total dE with PMT resolution correction
	Geant5::energy_t run();

private:
	energy_t tracePhoton(Photon photon);
	
	energy_t interactPhotoEffect(Photon& photon);
	energy_t interactComptonEffect(Photon& photon);
	energy_t interactPairProduction(Photon& photon);

	bool isPhotonInDetector(const Photon& photon) {
		return (photon.z_ >= 0.0) && (photon.z_ <= zMax_);
	}

	static Mu getLinearCoeffs(energy_t E) {
		return {
			0.1 / E * units::eMass,
			0.05 * E * units::eMass,
			(E >= 2.0 * units::eMass) ? 0.03 * (E - 2.0 * units::eMass) : 0.0 
		};
	}

	static double getPMTResolution(energy_t E) { return 0.1 * std::sqrt(E * units::eMass ); }

};


Geant5::energy_t Geant5::run() {

	energy_t dE{ tracePhoton({true, true, initialEnergy_, 0.0}) };
	
	// apply PMT noise
	gaussDist_.mu_ = dE;
	gaussDist_.sigma_ = getPMTResolution(dE);
	dE = gaussDist_.getValue();
	
	return dE;
}


Geant5::energy_t Geant5::tracePhoton(Photon photon) {
	energy_t dE{ 0.0 };
	
	// this is ugly af, but i'd rather already get it working
	while ( photon.isAlive_ ) {
		Mu mu{ getLinearCoeffs(photon.energy_) };

		length_t zPE = expDist_.getValue(mu.photoEffect);
		length_t zCE = expDist_.getValue(mu.comptonEffect);
		length_t zPP = expDist_.getValue(mu.pairProduction);
		
		if ( zPP < zPE && zPP < zCE && photon.energy_ >= 2.0 * units::eMass ) {
			// case for pair production
			photon.move(zPP);
			if ( isPhotonInDetector(photon) ) {
				dE += interactPairProduction(photon);
			}
			else {
				photon.isAlive_ = false;
			}
		
		}
		else {
			// cases for photo and compton effect
			if ( zPE < zCE ) {
				// case for photo
				photon.move(zPE);

				if ( isPhotonInDetector(photon) ) {
					dE += interactPhotoEffect(photon);
				}
				else {
					photon.isAlive_ = false;
				}
			}
			else {
				// case for compton
				photon.move(zCE);

				if ( isPhotonInDetector(photon) ) {
					dE += interactComptonEffect(photon);
				}
				else {
					photon.isAlive_ = false;
				}
			}
		}
	}
	
	return dE;
}


Geant5::energy_t Geant5::interactPhotoEffect(Photon& photon) {
	photon.isAlive_ = false;
	return photon.energy_;
}


Geant5::energy_t Geant5::interactComptonEffect(Photon& photon) {
	comptonDist_.setE(photon.energy_);
	energy_t dE{ photon.energy_ - comptonDist_.getValue() };
	photon.energy_ -= dE;
	return dE;
}


Geant5::energy_t Geant5::interactPairProduction(Photon& photon) {
	// original photon loses 2*eMass; electron and positron carry (E - 2) kinetic energy
	// and lose it all. dE = E - 2.
	energy_t dE{ photon.energy_ - 2.0 * units::eMass };
	// positron annihilates, total energy from this is 2*eMass
	// create and trace 2 photons: 1 forwards and 1 backwards
	dE += tracePhoton({ true, true, 1.0 * units::eMass, photon.z_ });
	dE += tracePhoton({ true, false, 1.0 * units::eMass, photon.z_ });
	photon.isAlive_ = false;
	return dE;
}


	
