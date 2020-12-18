#include <vector>
#include <iostream>
#include <random>
#include <algorithm>
#include <iomanip>

using namespace std;
typedef double meters_t;
typedef double eV_t;
typedef double us_t;

typedef std::exponential_distribution<double> dexp;
typedef std::uniform_real_distribution<double> duni;

struct Simulation {
    size_t n_points;
    meters_t depth_step;

    double *particles;
    double *pions;
    double *electrons;
    double *photons;
    double *muons;
    double *neutrinos;
    eV_t   *energy;

    std::mt19937 generator;

    meters_t c_us;

    size_t particles_total;

    Simulation (size_t n_points, meters_t depth_step) :
        n_points(n_points),
        depth_step(depth_step),
        particles_total(0)
    {
        this->particles = new double[n_points]();
        this->pions = new double[n_points]();
        this->electrons = new double[n_points]();
        this->photons = new double[n_points]();
        this->muons = new double[n_points]();
        this->neutrinos = new double[n_points]();
        this->energy = new eV_t[n_points]();

        this->c_us = 299.792458;
    }

    // distribute the quantity <total> among <count> buckets
    double* distribute(double total, size_t count, size_t gen) {
        const size_t c_max = 20;
        const size_t g_max = 50;

        // allocate this once
        static double random_distr[g_max][c_max];

        if (count > c_max) {
            cerr << "random_distr count > c_max" << endl;
            exit(1);
        }
        if (gen > g_max) {
            cerr << "random_distr gen > g_max" << endl;
            exit(1);
        }

        double sum = 0;
        for (int i = 0; i < count; i++) {
            double r = dexp(1)(generator);
            random_distr[gen][i] = r;
            sum += r;
        }
        sum /= total;
        for (int i = 0; i < count; i++) {
            random_distr[gen][i] /= sum;
        }
        // yes, this is horrible
        return random_distr[gen];
    }

    // count total number of particles processed
    void count_start (size_t gen) {
        particles_total++;
        if (particles_total % 100000 == 0) {
            cerr << setw(12) <<  particles_total << setw(4) << gen << endl;
        }
    }

    // register a particle in an array
    void trace (double *target, meters_t start, meters_t end, double value) {
        double start_norm = start / depth_step;
        double end_norm = end / depth_step;

        size_t i_min = start_norm, i_max = end_norm;

        if (i_min > i_max) {
            cerr << "i_min < i_max?" << endl;
            exit(1);
        }

        if (i_min >= n_points) {
            return;
        }

        if (i_min < 0) {
            i_min = 0;
            start_norm = 0;
        }

        if (i_max >= n_points) {
            i_max = n_points-1;
            end_norm = n_points;
        }

        for (size_t i = i_min; i < i_max; i++) {
            target[i] += value;
        }

        target[i_min] -= value * (start_norm - i_min);
        target[i_max] += value * (end_norm - i_max);
    }

    // simulate an electron or photon with energy <e> starting at depth d
    void emag (meters_t d, eV_t e, bool photon, size_t gen) {
        count_start(gen);

        meters_t λ_int = 3.039E+04 * 1e-2;
        if (photon) {
            λ_int *= 9.0 / 7;
        }

        meters_t interaction = dexp(1./λ_int)(generator);
        meters_t d_end = d + interaction;

        trace(energy, d, d_end, e);

        if (photon) {
            trace(photons, d, d_end, 1);
        } else {
            trace(electrons, d, d_end, 1);
        }

        if (e > 77e6) {
            if (photon) {
                emag(d_end, e/2, false, gen+1);
                emag(d_end, e/2, false, gen+1);
            } else {
                emag(d_end, e/2, true, gen+1);
                emag(d_end, e/2, false, gen+1);
            }
        }
    }

    // simulate a muon with energy <e> starting at depth d
    void muon (meters_t d, eV_t e, size_t gen) {
        count_start(gen);

        us_t τ = 2.2;

        meters_t decay = dexp(1./τ)(generator) * c_us * e / 105.6e6;
        meters_t d_end = d + decay;

        trace(energy, d, d_end, e);
        trace(muons, d, d_end, 1);

        neutrino(d_end, e/2, gen+1);
        emag(d_end, e/2, false, gen+1);
    }

    // simulate a neutrino with energy <e> starting at depth d
    void neutrino (meters_t d, eV_t e, size_t gen) {
        count_start(gen);

        trace(neutrinos, d, d + depth_step * n_points, 1);
    }

    // simulate a pion with energy <e> starting at depth d
    void pion (meters_t d, eV_t e, size_t gen) {
        count_start(gen);

        us_t τ = 2.6033e-2;
        meters_t λ_int = 7.477E+04 * 1e-2;

        meters_t decay = dexp(1./τ)(generator) * c_us * e / 139.6e6;
        meters_t interaction = dexp(1./λ_int)(generator);

        meters_t d_end = d + min(decay, interaction);

        trace(energy, d, d_end, e);
        trace(pions, d, d_end, 1);

        if (interaction < decay) {
            size_t N_ch = 10, N_0 = 5, N_tot = N_ch + N_0;
            // produce new charged pions
            for (int i = 0; i < N_ch; i++) {
                pion(d_end, e / N_tot, gen+1);
            }
            // produce 2*N_0 new photons
            for (int i = 0; i < (2*N_0); i++) {
                emag(d_end, e / N_tot / 2, true, gen+1);
            }
        } else {
            neutrino(d_end, e/2, gen+1);
            muon(d_end, e/2, gen+1);
        }
    };
};

int main() {
    size_t n_points = 300;
    meters_t depth_total = 20000;

    auto s = new Simulation(n_points, depth_total / n_points);
   
    // s->emag(0, 1e15, true);
    s->pion(0, 1e14, 0);

    for (size_t i = 0; i < n_points; i++)  {
        cout << i * s->depth_step << '\t';
        cout << s->particles[i] << '\t';
        cout << s->pions[i] << '\t';
        cout << s->electrons[i] << '\t';
        cout << s->photons[i] << '\t';
        cout << s->muons[i] << '\t';
        cout << s->neutrinos[i] << '\t';
        cout << s->energy[i];
        cout << endl;
    }
    cout << endl;
};