#pragma once

#include "Application.hpp"
#include "Config.hpp"
#include "Loader.hpp"
#include "SwitchManager.hpp"
#include "OFServer.hpp"
#include "api/SwitchFwd.hpp"
#include "oxm/openflow_basic.hh"

namespace runos {

using SwitchPtr = safe::shared_ptr<Switch>;
namespace of13 = fluid_msg::of13;

class DatasetCollector : public Application
{
    Q_OBJECT
    SIMPLE_APPLICATION(DatasetCollector, "dataset-collector")
public:
    void init(Loader* loader, const Config& config) override;
    void startUpInactive(Loader* loader);

private:
    OFMessageHandlerPtr handler_;
    SwitchManager* switch_manager_;
    OFServer* of_server_;
	
    boost::chrono::seconds data_pickup_period_;
    boost::thread data_pickup_thread_;
};

} // namespace runos
