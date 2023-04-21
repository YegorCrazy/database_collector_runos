#include "DatasetCollector.hpp"

#include "PacketParser.hpp"
#include "api/Packet.hpp"
#include "CommandLine.hpp"
#include "SwitchImpl.hpp"
#include "OFAgentImpl.hpp"
#include <runos/core/logging.hpp>
#include <fluid/of13msg.hh>

#include <string>
#include <cmath>
#include <boost/chrono.hpp>

namespace runos {

REGISTER_APPLICATION(DatasetCollector, {"controller",
                                "switch-manager",
                                "topology",
                                "of-server",
                                "command-line",
                                ""})
                                
void DatasetCollector::init(Loader* loader, const Config& config) {
    switch_manager_ = SwitchManager::get(loader);
    of_server_ = OFServer::get(loader);
    CommandLine* cli = CommandLine::get(loader);
    // registering command: collect dataset n filename.csv
    // adding n rows to file "filename.csv"
    cli->register_command(
        cli_pattern(R"(collect\s+dataset\s+([0-9]+)\s+(.+\.csv))"),
        [=](cli_match const& match) {
            std::ofstream file;
            file.open(match[2], std::ios::app);
            this->CollectFlowsInfo(std::stoi(match[1]), file);
            file.close();
        });
    data_pickup_period_ = boost::chrono::seconds(config_get(
        config_cd(config, "dataset-collector"), "data-pickup-period", 3));
        
    handler_ = Controller::get(loader)->register_handler(
    [=](of13::PacketIn& fr, OFConnectionPtr ofconn) mutable -> bool
    {
        flows_removed += 1;
        LOG(INFO) << "Flow " << fr.cookie() << " removed, count is " << flows_removed;
        
        packets_in_removed_flow_[fr.cookie()] = fr.packet_count();

        return false;
    }, -4);
}

void DatasetCollector::CollectFlowsInfo(int iter_num, std::ofstream& file) {
    for (int i = 0; i < iter_num; ++i) {
        // truly there is must be only one switch
        for (auto switch_ptr : switch_manager_->switches()) {
            auto dpid = switch_ptr->dpid();
            auto of_agent_future = of_server_->agent(dpid);
            of_agent_future.wait();
            auto of_agent = of_agent_future.get();
            
            ofp::flow_stats_request req;
            req.out_port = of13::OFPP_ANY;
            req.out_group = of13::OFPG_ANY;
            req.cookie = 1;
            req.cookie_mask = 0x00000000FFFFFFFFULL;
            
            auto response_future = of_agent->request_flow_stats(req);
            response_future.wait();
            auto response = response_future.get();
            
            auto FlowCount = response.size();
            if (FlowCount == 0) {
                continue;
            }
            
            long long SpeedOfFlowEntries = FlowCount - flows_num + flows_removed;
            flows_num = FlowCount;
            
            long long sum_packet_count = 0;
            std::unordered_map<uint64_t, long long> new_packets_in_flows;
            for (auto flow_stat : response) {
                auto cookie = flow_stat.cookie();
                if (packets_in_flow_.find(cookie) != packets_in_flow_.end()) {
                    auto new_packets = flow_stat.packet_count() - packets_in_flow_[cookie];
                    sum_packet_count += new_packets;
                    new_packets_in_flows[cookie] = new_packets;
                } else {
                    sum_packet_count += flow_stat.packet_count();
                    new_packets_in_flows[cookie] = flow_stat.packet_count();
                }
                packets_in_flow_[cookie] = flow_stat.packet_count();
            }
            for (auto [cookie, packets_num] : packets_in_removed_flow_) {
                long long new_packets;
                if (packets_in_flow_.find(cookie) != packets_in_flow_.end()) {
                    new_packets = packets_num - packets_in_flow_[cookie];
                    packets_in_flow_.erase(cookie);
                } else {
                    new_packets = packets_num;
                }
                sum_packet_count += new_packets;
                new_packets_in_flows[cookie] = new_packets;
            }
            long long flows_total = FlowCount + packets_in_removed_flow_.size();
            long double AverageNumberOfFlowPackets = double(sum_packet_count) / flows_total;
            
            long double VariationNumberOfFlowPackets = 0;
            for (auto [_, packets_num] : new_packets_in_flows) {
                VariationNumberOfFlowPackets += std::pow(
                    packets_num - AverageNumberOfFlowPackets,
                    2);
            }
            VariationNumberOfFlowPackets = std::sqrt(VariationNumberOfFlowPackets / flows_total);
            
            file << FlowCount << "," << SpeedOfFlowEntries << "," 
                 << AverageNumberOfFlowPackets << "," 
                 << VariationNumberOfFlowPackets << std::endl;
        }
        packets_in_removed_flow_.clear();
        flows_removed = 0;
        
        boost::this_thread::sleep_for(data_pickup_period_);
    }
    LOG(INFO) << "all info substracted";
}

} // namespace runos
