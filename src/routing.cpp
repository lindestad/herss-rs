#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <array>
#include <cmath>
#include <iomanip>
#include <stdexcept>

struct TimeStep {
    double dt_hours;
    double q_in_m3s;
};

struct ReservoirState {
    double storage_m3 = 0.0;
};

struct ReservoirStepResult {
    double storage_new_m3;
    double q_out_avg_m3s;
    double q_out_end_m3s;
};

std::vector<TimeStep> loadInflowFile(const std::string& filename) {
    std::ifstream in(filename);
    if (!in.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    std::vector<TimeStep> series;
    std::string line;
    size_t line_no = 0;

    while (std::getline(in, line)) {
        ++line_no;

        size_t hash_pos = line.find('#');
        if (hash_pos != std::string::npos) {
            line.erase(hash_pos);
        }

        if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }

        std::istringstream iss(line);
        TimeStep step{};

        if (!(iss >> step.dt_hours >> step.q_in_m3s)) {
            if (line_no == 1) {
                continue;
            }
            throw std::runtime_error(
                "Parse error in " + filename + " at line " + std::to_string(line_no)
            );
        }

        if (step.dt_hours <= 0.0) {
            throw std::runtime_error(
                "dt_hours must be positive in " + filename + " at line " + std::to_string(line_no)
            );
        }

        if (step.q_in_m3s < 0.0) {
            throw std::runtime_error(
                "q_in_m3s must be non-negative in " + filename + " at line " + std::to_string(line_no)
            );
        }

        series.push_back(step);
    }

    if (series.empty()) {
        throw std::runtime_error("No valid data rows in file: " + filename);
    }

    return series;
}

class ThreeReservoirCascade {
public:
    explicit ThreeReservoirCascade(double k_total_hours) {
        k_total_seconds_ = k_total_hours * 3600.0;
        k_res_seconds_ = k_total_seconds_ / 3.0;
    }

    void setInitialStorage(double total_storage_m3) {
        double share = total_storage_m3 / 3.0;
        for (size_t i = 0; i < reservoirs_.size(); ++i) {
            reservoirs_[i].storage_m3 = share;
        }
    }

    double route(double q_in_m3s, double dt_seconds) {
        double q_for_next = q_in_m3s;

        for (size_t i = 0; i < reservoirs_.size(); ++i) {
            ReservoirStepResult result =
                routeOneReservoir(reservoirs_[i].storage_m3, q_for_next, k_res_seconds_, dt_seconds);

            reservoirs_[i].storage_m3 = result.storage_new_m3;
            q_for_next = result.q_out_avg_m3s;
        }

        return q_for_next; // average outlet discharge over the step
    }

    double totalStorageM3() const {
        double sum = 0.0;
        for (size_t i = 0; i < reservoirs_.size(); ++i) {
            sum += reservoirs_[i].storage_m3;
        }
        return sum;
    }

    double reservoirKHours() const {
        return k_res_seconds_ / 3600.0;
    }

private:
    ReservoirStepResult routeOneReservoir(
        double storage_old_m3,
        double q_in_m3s,
        double k_seconds,
        double dt_seconds)
    {
        double factor = std::exp(-dt_seconds / k_seconds);

        double storage_new_m3 =
            storage_old_m3 * factor
            + k_seconds * (1.0 - factor) * q_in_m3s;

        double volume_out_m3 =
            storage_old_m3 + q_in_m3s * dt_seconds - storage_new_m3;

        double q_out_avg_m3s = volume_out_m3 / dt_seconds;
        double q_out_end_m3s = storage_new_m3 / k_seconds;

        if (q_out_avg_m3s < 0.0) q_out_avg_m3s = 0.0;
        if (q_out_end_m3s < 0.0) q_out_end_m3s = 0.0;

        return {storage_new_m3, q_out_avg_m3s, q_out_end_m3s};
    }

    std::array<ReservoirState, 3> reservoirs_;
    double k_total_seconds_ = 0.0;
    double k_res_seconds_ = 0.0;
};

int main() {
    try {
        const std::string inflow_filename = "inflow.txt";
        const std::string out_filename = "res.txt";

        const double k_total_hours = 5.0;
        const double initial_storage_m3 = 100000.0;

        std::vector<TimeStep> series = loadInflowFile(inflow_filename);

        ThreeReservoirCascade channel(k_total_hours);
        channel.setInitialStorage(initial_storage_m3);

        std::ofstream out(out_filename);
        if (!out.is_open()) {
            throw std::runtime_error("Could not open output file: " + out_filename);
        }

        out << std::fixed << std::setprecision(6);
        out << "step\tdt_hours\tQin_m3s\tQout_m3s\tstorage_m3\n";

        double total_in_m3 = 0.0;
        double total_out_m3 = 0.0;

        for (size_t i = 0; i < series.size(); ++i) {
            
            double dt_seconds = series[i].dt_hours * 3600.0;
            double q_in = series[i].q_in_m3s;
            double q_out = channel.route(q_in, dt_seconds);
            double storage_m3 = channel.totalStorageM3();

            total_in_m3 += q_in * dt_seconds;
            total_out_m3 += q_out * dt_seconds;

            out << (i + 1) << "\t"
                << series[i].dt_hours << "\t"
                << q_in << "\t"
                << q_out << "\t"
                << storage_m3 << "\n";
        }

        out.close();

        double final_storage_m3 = channel.totalStorageM3();
        double water_balance_m3 = initial_storage_m3 + total_in_m3 - total_out_m3 - final_storage_m3;

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "3-reservoir cascade routing completed\n";
        std::cout << "K_total_hours      = " << k_total_hours << "\n";
        std::cout << "K_reservoir_hours  = " << channel.reservoirKHours() << "\n";
        std::cout << "Output written to  = " << out_filename << "\n";
        std::cout << "Initial storage    = " << initial_storage_m3 << " m3\n";
        std::cout << "Total inflow       = " << total_in_m3 << " m3\n";
        std::cout << "Total outflow      = " << total_out_m3 << " m3\n";
        std::cout << "Final storage      = " << final_storage_m3 << " m3\n";
        std::cout << "Water balance      = " << water_balance_m3 << " m3\n";

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}