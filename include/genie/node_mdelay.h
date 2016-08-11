#pragma once

#include "genie/structure.h"
#include "genie/protocol.h"

namespace genie
{
    class RVDPort;
    class RVDLink;

    class NodeMDelay : public Node
    {
    public:
        NodeMDelay();

        bool is_interconnect() const override { return true; }

        RVDPort* get_input() const;
        RVDPort* get_output() const;

        void set_delay(unsigned delay);
        PROP_GET(delay, unsigned, m_delay);

        HierObject* instantiate() override;
        void do_post_carriage() override;

        AreaMetrics get_area_usage() const override;

    protected:
        RVDLink* m_int_link;
        unsigned m_delay;
        CarrierProtocol m_proto;
    };
}