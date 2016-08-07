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

        RVDPort* get_input() const;
        RVDPort* get_output() const;

        void set_delay(unsigned delay);
        PROP_GET(delay, unsigned, m_delay);

        HierObject* instantiate() override;
        void do_post_carriage() override;

    protected:
        RVDLink* m_int_link;
        unsigned m_delay;
        CarrierProtocol m_proto;
    };
}