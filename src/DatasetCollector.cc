#include "DatasetCollector.hpp"

#include "PacketParser.hpp"
#include "api/Packet.hpp"
#include "CommandLine.hpp"
#include "SwitchImpl.hpp"
#include "OFAgentImpl.hpp"
#include <runos/core/logging.hpp>
#include <fluid/of13msg.hh>

#include <string>
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
    cli->register_command(
        cli_pattern(R"(collect\s+dataset\s+([0-9]+))"),
        [=](cli_match const& match) {
            this->CollectFlowsInfo(std::stoi(match[1]));
        });
    data_pickup_period_ = boost::chrono::seconds(config_get(
        config_cd(config, "dataset-collector"), "data-pickup-period", 3));
}

void DatasetCollector::CollectFlowsInfo(int iter_num) {
    for (int i = 0; i < iter_num; ++i) {
        for (auto switch_ptr : switch_manager_->switches()) {
            auto dpid = switch_ptr->dpid();
            auto of_agent_future = of_server_->agent(dpid);
            of_agent_future.wait();
            auto of_agent = of_agent_future.get();
            
            ofp::flow_stats_request req;
            req.out_port = of13::OFPP_ANY;
            req.out_group = of13::OFPG_ANY;
            req.cookie = 1;
            req.cookie_mask = 0xFFFFFFFFFFFFFFFFULL;
            
            auto response_future = of_agent->request_flow_stats(req);
            response_future.wait();
            auto response = response_future.get();
            for (auto flow_stat : response) {
                LOG(INFO) << "from switch " << dpid << 
                    " got info: " << flow_stat.packet_count();
            }
        }
        boost::this_thread::sleep_for(data_pickup_period_);
    }
    LOG(INFO) << "all info substracted";
}

} // namespace runos
