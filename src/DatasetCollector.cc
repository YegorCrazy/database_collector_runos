#include "DatasetCollector.hpp"

#include "PacketParser.hpp"
#include "api/Packet.hpp"
#include "SwitchImpl.hpp"
#include "OFAgentImpl.hpp"
#include <runos/core/logging.hpp>
#include <fluid/of13msg.hh>

namespace runos {

REGISTER_APPLICATION(DatasetCollector, {"controller",
                                "switch-manager",
                                "topology",
                                "of-server"
                                ""})
                                
void DatasetCollector::init(Loader* loader, const Config& config) {
    // LOG(INFO) << "started initing";
    switch_manager_ = SwitchManager::get(loader);
    of_server_ = OFServer::get(loader);
    data_pickup_period_ = boost::chrono::seconds(config_get(
        config_cd(config, "dataset-collector"), "data-pickup-period", 3));
    LOG(INFO) << "Data pickup period is " << data_pickup_period_.count();
    // LOG(INFO) << "ended initing";
}

void DatasetCollector::startUpInactive(Loader* loader) {
    for (auto switch_ptr : switch_manager_->switches()) {
        auto dpid = switch_ptr->dpid();
        auto of_agent_future = of_server_->agent(dpid);
        of_agent_future.wait();
        auto of_agent = of_agent_future.get();
        
        ofp::flow_stats_request req;
        req.out_port = of13::OFPP_ANY;
        req.out_group = of13::OFPG_ANY;
        
        auto response_future = of_agent->request_flow_stats(req);
        response_future.wait();
        auto response = response_future.get();
        for (auto flow_stat : response) {
            LOG(INFO) << "Got info: " << flow_stat.hard_timeout();
        }
    }
}

} // namespace runos
