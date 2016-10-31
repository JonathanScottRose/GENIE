#include "flow.h"
#include "genie/net_rs.h"
#include "genie/net_rvd.h"
#include "lp_lib.h"

using namespace genie;
using namespace graphs;
using namespace flow;

namespace genie
{
namespace flow
{

void apply_latency_constraints(System* sys)
{
    auto aconstraints = sys->asp_get<ARSLatencyConstraints>();
    if (!aconstraints)
        return;

    // LPSolve-specific versions of constraint
    struct RowConstraint
    {
        std::vector<double> coef;
        std::vector<int> varno;
        int op;
        int rhs;
    };
    
    std::vector<RowConstraint> lp_constraints;

    // Holds mapping between free RVD links and column variables
    std::unordered_map<RVDLink*, int> link_to_varno;
    int next_varno = 1;

    // Process each constraint
    for (auto& constraint : aconstraints->constraints)
    {
        lp_constraints.resize(lp_constraints.size() + 1);

        // Current constraint
        RowConstraint& cur_row = lp_constraints.back();

        // Sum of constants on LHS of inequality
        int row_lhs = 0;

        // Process each path term in the constraint
        for (auto& path_term : constraint.path_terms)
        {
            auto path_sign = path_term.first;
            RSPath& path = path_term.second;

            // Used to chain each RS link's source with the sink of the previous
            RSPort* last_rs_sink = nullptr;

            // Walk each RS Link in the path
            for (auto& rs_link : path)
            {
                if (last_rs_sink)
                {
                    auto cur_rs_src = RSPort::get_rs_port(rs_link->get_src());

                    auto int_links = last_rs_sink->get_node()->get_links(last_rs_sink, cur_rs_src);

                    if (int_links.empty())
                    {
                        throw HierException(cur_rs_src->get_node(), 
                            "can't process latency constraint because there exists no internal RS Link "
                            "between '" + last_rs_sink->get_name() + "' and '" + cur_rs_src->get_name() + "'");
                    }

                    // Internal edges contribute a constant latency term to the LHS of the inequality
                    auto int_link = (RSLink*)int_links.front();
                    row_lhs += path_sign==RSLatencyConstraint::PLUS?
                        int_link->get_latency() : -int_link->get_latency();
                }

                // loop over child RVDEdges, get their latencies, add to constraint
                auto acont = rs_link->asp_get<ALinkContainment>();
                for (auto child_link : acont->get_all_child_links(NET_RVD))
                {
                    auto rvd_link = (RVDLink*)child_link;

                    // Internal link? Add latency to LHS. Otherwise, it becomes a variable to solve for
                    bool internal = rvd_link->get_src()->get_node() == rvd_link->get_sink()->get_node();
                    if (internal)
                    {
                        row_lhs += path_sign==RSLatencyConstraint::PLUS?
                            rvd_link->get_latency() : -rvd_link->get_latency();
                    }
                    else
                    {
                        // create/get 'column' variable
                        auto varit = link_to_varno.find(rvd_link);
                        if (varit == link_to_varno.end())
                            varit = link_to_varno.emplace(rvd_link, next_varno++).first;
                        int varno = varit->second;

                        cur_row.coef.resize(next_varno-1);
                        cur_row.varno.resize(next_varno-1);

                        // add coefficient (+1/-1) to the row
                        cur_row.coef[varno-1] += path_sign == RSLatencyConstraint::PLUS? 1.0 : -1.0;
                        cur_row.varno[varno-1] = varno;
                    }
                }

                last_rs_sink = RSPort::get_rs_port(rs_link->get_sink());
            }
        } // end path terms

        // LHS is done. Set up row's comparison function and RHS
        cur_row.rhs = constraint.rhs;
        cur_row.rhs -= row_lhs;
        
        switch (constraint.op)
        {
        case RSLatencyConstraint::LT:
            cur_row.rhs--;
        case RSLatencyConstraint::LEQ:
            cur_row.op = ROWTYPE_LE;
            break;
        case RSLatencyConstraint::CompareOp::EQ:
            cur_row.op = ROWTYPE_EQ;
            break;
        case RSLatencyConstraint::GT:
            cur_row.rhs++;
        case RSLatencyConstraint::GEQ:
            cur_row.op = ROWTYPE_GE;
            break;
        }
    }// for each constraint

     // Set up lp_solve problem
    lprec* lp_prob = make_lp(lp_constraints.size(), next_varno-1);
    if (!lp_prob)
        throw HierException(sys, "couldn't create lp_solve problem");

    set_verbose(lp_prob, NEUTRAL);
    put_logfunc(lp_prob, nullptr, nullptr);
    put_msgfunc(lp_prob, nullptr, nullptr, -1);
    set_presolve(lp_prob, PRESOLVE_ROWS | PRESOLVE_COLS, get_presolveloops(lp_prob));
    set_add_rowmode(lp_prob, TRUE);

    // Set the objective function to minimize
    {
        std::vector<double> cns_coef;
        std::vector<int> cns_varno;

        for (auto it : link_to_varno)
        {
            RVDLink* link = it.first;
            int varno = it.second;

            // Coefficient = width of RVD link in bits.
            // Add 1 to width to include overhead of valid bits, etc
            int cost = link->get_width() + 1;

            cns_coef.push_back((double)cost);
            cns_varno.push_back(varno);
        }

        if (!set_obj_fnex(lp_prob, cns_coef.size(), cns_coef.data(), cns_varno.data()))
            throw HierException(sys, "failed setting objective function");
    }

    // Register all constraints
    for (auto& row : lp_constraints)
    {
        if (!add_constraintex(lp_prob, row.coef.size(), row.coef.data(), row.varno.data(), 
            row.op, (double)row.rhs))
        {
            throw HierException(sys, "failed adding row constraint");
        }
    }

    // Make all variables integers
    set_add_rowmode(lp_prob, FALSE);
    for (int i = 1; i < next_varno; i++)
    {
        set_int(lp_prob, i, TRUE);
    }

    // Solve and get results
    set_minim(lp_prob);
    switch (solve(lp_prob))
    {
    case NOMEMORY: throw HierException(sys, "lpsolve: out of memory"); break;
    case NOFEASFOUND:
    case INFEASIBLE: throw HierException(sys, "lpsolve: infeasible solution"); break;
    case DEGENERATE:
    case NUMFAILURE:
    case PROCFAIL: throw HierException(sys, "lpsolve: misc failure"); break;
    default: break;
    }

    // Get results and set latencies
    {
        int nrows = get_Norig_rows(lp_prob);
        int ncols = get_Norig_columns(lp_prob);

        auto varno_to_link = util::reverse_map(link_to_varno);

        for (int col = 1; col <= ncols; col++)
        {
            int latency = (int)get_var_primalresult(lp_prob, nrows + col);

            RVDLink* link = varno_to_link[col].front();
            link->set_latency(latency);
        }
    }

    delete_lp(lp_prob);
}

} // end namespaces
}