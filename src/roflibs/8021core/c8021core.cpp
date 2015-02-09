/*
 * c8021core.cpp
 *
 *  Created on: 03.08.2014
 *      Author: andreas
 */

#include "c8021core.hpp"

using namespace roflibs::c8021;

/*static*/std::map<rofl::cdptid, c8021core*> c8021core::c8021cores;
/*static*/std::set<uint64_t> c8021core::dpids;
/*static*/std::string c8021core::script_path_port_up 	= std::string("/var/lib/basebox/port-up.sh");
/*static*/std::string c8021core::script_path_port_down 	= std::string("/var/lib/basebox/port-down.sh");

c8021core::c8021core(const rofl::cdptid& dptid,
		uint8_t table_id_eth_in, uint8_t table_id_eth_src,
		uint8_t table_id_eth_local, uint8_t table_id_eth_dst,
		uint16_t default_pvid) :
			state(STATE_IDLE),
			cookie_grp_addr(roflibs::common::openflow::ccookie_owner::acquire_cookie()),
			cookie_miss_entry_src(roflibs::common::openflow::ccookie_owner::acquire_cookie()),
			cookie_miss_entry_local(roflibs::common::openflow::ccookie_owner::acquire_cookie()),
			cookie_redirect_inject(roflibs::common::openflow::ccookie_owner::acquire_cookie()),
			dptid(dptid), default_pvid(default_pvid),
			table_id_eth_in(table_id_eth_in),
			table_id_eth_src(table_id_eth_src),
			table_id_eth_local(table_id_eth_local),
			table_id_eth_dst(table_id_eth_dst) {

	if (c8021core::c8021cores.find(dptid) != c8021core::c8021cores.end()) {
		throw e8021CoreExists("c8021core::c8021core() dpid already exists");
	}
}


c8021core::~c8021core() {
	try {
		if (STATE_ATTACHED == state) {
			handle_dpt_close();
		}
	} catch (rofl::eRofDptNotFound& e) {}

	clear_tap_devs(dptid);
}



void
c8021core::handle_dpt_open()
{
	try {
		state = STATE_ATTACHED;

		rofl::crofdpt& dpt = rofl::crofdpt::get_dpt(dptid);

		// install 01:80:c2:xx:xx:xx entry in table 0
		rofl::openflow::cofflowmod fm(dpt.get_version_negotiated());
		fm.set_command(rofl::openflow::OFPFC_ADD);
		fm.set_priority(0xf000);
		fm.set_cookie(cookie_grp_addr);
		fm.set_table_id(table_id_eth_in);
		fm.set_match().set_eth_dst(rofl::caddress_ll("01:80:c2:00:00:00"), rofl::caddress_ll("ff:ff:ff:00:00:00"));
		fm.set_instructions().set_inst_apply_actions().set_actions().
				add_action_output(rofl::cindex(0)).set_port_no(rofl::openflow::OFPP_CONTROLLER);
		fm.set_instructions().set_inst_apply_actions().set_actions().
				set_action_output(rofl::cindex(0)).set_max_len(1526);
		dpt.send_flow_mod_message(rofl::cauxid(0), fm);

		// install miss entry for src stage table
		fm.clear();
		fm.set_command(rofl::openflow::OFPFC_ADD);
		fm.set_priority(0x1000);
		fm.set_cookie(cookie_miss_entry_src);
		fm.set_table_id(table_id_eth_src);
		fm.set_instructions().set_inst_apply_actions().set_actions().
				add_action_output(rofl::cindex(0)).set_port_no(rofl::openflow::OFPP_CONTROLLER);
		fm.set_instructions().set_inst_apply_actions().set_actions().
				set_action_output(rofl::cindex(0)).set_max_len(1526);
		fm.set_instructions().set_inst_goto_table().set_table_id(table_id_eth_local/*2*/);
		dpt.send_flow_mod_message(rofl::cauxid(0), fm);

		// install miss entry for local address stage table (src-stage + 1)
		fm.clear();
		fm.set_command(rofl::openflow::OFPFC_ADD);
		fm.set_priority(0x1000);
		fm.set_cookie(cookie_miss_entry_local);
		fm.set_table_id(table_id_eth_src+1);
		fm.set_instructions().set_inst_goto_table().set_table_id(table_id_eth_dst);
		dpt.send_flow_mod_message(rofl::cauxid(0), fm);

		// forward packets received from special port OFPP_CONTROLLER to table_id_eth_dst
		fm.set_table_id(table_id_eth_in);
		fm.set_priority(0x8200);
		fm.set_cookie(cookie_redirect_inject);
		fm.set_match().clear();
		fm.set_match().set_in_port(rofl::openflow::OFPP_CONTROLLER);
		fm.set_instructions().set_inst_goto_table().set_table_id(table_id_eth_dst);
		dpt.send_flow_mod_message(rofl::cauxid(0), fm);

	} catch (rofl::eRofSockTxAgain& e) {
		rofcore::logging::debug << "[c8021core][handle_dpt_open] control channel congested" << std::endl;
	} catch (rofl::eRofBaseNotConnected& e) {
		rofcore::logging::debug << "[c8021core][handle_dpt_open] control channel not connected" << std::endl;
	}
}



void
c8021core::handle_dpt_close()
{
	try {
		state = STATE_DETACHED;

		rofl::crofdpt& dpt = rofl::crofdpt::get_dpt(dptid);

		// remove miss entry for src stage table
		rofl::openflow::cofflowmod fm(dpt.get_version_negotiated());
		fm.set_command(rofl::openflow::OFPFC_DELETE_STRICT);
		fm.set_priority(0xf000);
		fm.set_cookie(cookie_miss_entry_src);
		fm.set_table_id(table_id_eth_src);
		dpt.send_flow_mod_message(rofl::cauxid(0), fm);

		// remove miss entry for local address stage table (src-stage + 1)
		fm.clear();
		fm.set_command(rofl::openflow::OFPFC_DELETE_STRICT);
		fm.set_priority(0x1000);
		fm.set_cookie(cookie_miss_entry_local);
		fm.set_table_id(table_id_eth_src+1);
		dpt.send_flow_mod_message(rofl::cauxid(0), fm);

		// remove 01:80:c2:xx:xx:xx entry from table 0
		fm.clear();
		fm.set_command(rofl::openflow::OFPFC_DELETE_STRICT);
		fm.set_priority(0xf000);
		fm.set_cookie(cookie_grp_addr);
		fm.set_table_id(table_id_eth_in);
		fm.set_match().set_eth_dst(rofl::caddress_ll("01:80:c2:00:00:00"), rofl::caddress_ll("ff:ff:ff:00:00:00"));
		dpt.send_flow_mod_message(rofl::cauxid(0), fm);

		// forward packets received from special port OFPP_CONTROLLER to table_id_eth_dst
		fm.set_table_id(table_id_eth_in);
		fm.set_priority(0x8200);
		fm.set_cookie(cookie_redirect_inject);
		fm.set_match().clear();
		fm.set_match().set_in_port(rofl::openflow::OFPP_CONTROLLER);
		dpt.send_flow_mod_message(rofl::cauxid(0), fm);


	} catch (rofl::eRofSockTxAgain& e) {
		rofcore::logging::debug << "[c8021core][handle_dpt_close] control channel congested" << std::endl;
	} catch (rofl::eRofBaseNotConnected& e) {
		rofcore::logging::debug << "[c8021core][handle_dpt_close] control channel not connected" << std::endl;
	}
}



void
c8021core::handle_packet_in(rofl::crofdpt& dpt, const rofl::cauxid& auxid, rofl::openflow::cofmsg_packet_in& msg)
{
	try {
		rofcore::logging::debug << "[c8021core][handle_packet_in] pkt received: " << std::endl << msg;

		// yet unknown source address found in eth-src stage
		if (msg.get_table_id() == table_id_eth_src) {


		// all other stages: send frame to tap devices
		} else {

			if (not msg.get_match().has_vlan_vid()) {
				// no VLAN related metadata found, ignore packet
				rofcore::logging::debug << "[c8021core][handle_packet_in] frame without VLAN tag received, dropping" << std::endl;
				dpt.drop_buffer(auxid, msg.get_buffer_id());
				return;
			}

			if (not msg.get_match().get_eth_dst().is_multicast()) {
				/* non-multicast frames are directly injected into a tapdev */
				rofl::cpacket *pkt = rofcore::cpacketpool::get_instance().acquire_pkt();
				*pkt = msg.get_packet();
				pkt->pop(sizeof(struct rofl::fetherframe::eth_hdr_t)-sizeof(uint16_t), sizeof(struct rofl::fvlanframe::vlan_hdr_t));
				set_tap_dev(dptid, msg.get_match().get_eth_dst()).enqueue(pkt);

			} else {
				/* multicast frames carry a metadata field with the VLAN id
				 * for both tagged and untagged frames. Lookup all tapdev
				 * instances belonging to the specified vid and clone the packet
				 * for all of them. */

				uint16_t vid = msg.get_match().get_vlan_vid() & 0x0fff;

				for (std::map<std::string, rofcore::ctapdev*>::iterator
						it = devs[dptid].begin(); it != devs[dptid].end(); ++it) {
					rofcore::ctapdev& tapdev = *(it->second);
					if (tapdev.get_pvid() != vid) {
						continue;
					}
					rofl::cpacket *pkt = rofcore::cpacketpool::get_instance().acquire_pkt();
					*pkt = msg.get_packet();
					// offset: 12 bytes (eth-hdr without type), nbytes: 4 bytes
					pkt->pop(sizeof(struct rofl::fetherframe::eth_hdr_t)-sizeof(uint16_t), sizeof(struct rofl::fvlanframe::vlan_hdr_t));
					tapdev.enqueue(pkt);
				}
			}

			// drop buffer on data path, if the packet was stored there, as it is consumed entirely by the underlying kernel
			if (rofl::openflow::base::get_ofp_no_buffer(dpt.get_version_negotiated()) != msg.get_buffer_id()) {
				dpt.drop_buffer(auxid, msg.get_buffer_id());
			}
		}


	} catch (rofl::openflow::eOxmNotFound& e) {
		rofcore::logging::debug << "[c8021core][handle_packet_in] no VID match found" << std::endl;
	}
}



void
c8021core::handle_flow_removed(rofl::crofdpt& dpt, const rofl::cauxid& auxid, rofl::openflow::cofmsg_flow_removed& msg)
{
	try {


	} catch (rofl::openflow::eOxmNotFound& e) {
		rofcore::logging::debug << "[c8021core][handle_flow_removed] no VID match found" << std::endl;
	}
}



void
c8021core::handle_port_status(rofl::crofdpt& dpt, const rofl::cauxid& auxid, rofl::openflow::cofmsg_port_status& msg)
{

}



void
c8021core::handle_error_message(rofl::crofdpt& dpt, const rofl::cauxid& auxid, rofl::openflow::cofmsg_error& msg)
{

}


void
c8021core::enqueue(rofcore::cnetdev *netdev, rofl::cpacket* pkt)
{
	try {
		rofcore::ctapdev* tapdev = dynamic_cast<rofcore::ctapdev*>( netdev );
		if (0 == tapdev) {
			throw eLinkTapDevNotFound("c8021core::enqueue() tap device not found");
		}

		rofl::crofdpt& dpt = rofl::crofdpt::get_dpt(tapdev->get_dptid());

		if (not dpt.is_established()) {
			throw eLinkNoDptAttached("c8021core::enqueue() dpt not found");
		}

		rofl::openflow::cofactions actions(dpt.get_version_negotiated());
		actions.set_action_push_vlan(rofl::cindex(0)).set_eth_type(rofl::fvlanframe::VLAN_CTAG_ETHER);
		actions.set_action_set_field(rofl::cindex(1)).set_oxm(rofl::openflow::coxmatch_ofb_vlan_vid(tapdev->get_pvid()));
		actions.set_action_output(rofl::cindex(2)).set_port_no(rofl::openflow::OFPP_TABLE);

		dpt.send_packet_out_message(
				rofl::cauxid(0),
				rofl::openflow::base::get_ofp_no_buffer(dpt.get_version_negotiated()),
				rofl::openflow::base::get_ofpp_controller_port(dpt.get_version_negotiated()),
				actions,
				pkt->soframe(),
				pkt->length());

	} catch (rofl::eRofDptNotFound& e) {
		rofcore::logging::error << "[c8021core][enqueue] no data path attached, dropping outgoing packet" << std::endl;

	} catch (eLinkNoDptAttached& e) {
		rofcore::logging::error << "[c8021core][enqueue] no data path attached, dropping outgoing packet" << std::endl;

	} catch (eLinkTapDevNotFound& e) {
		rofcore::logging::error << "[c8021core][enqueue] unable to find tap device" << std::endl;
	}

	rofcore::cpacketpool::get_instance().release_pkt(pkt);
}



void
c8021core::enqueue(rofcore::cnetdev *netdev, std::vector<rofl::cpacket*> pkts)
{
	for (std::vector<rofl::cpacket*>::iterator
			it = pkts.begin(); it != pkts.end(); ++it) {
		enqueue(netdev, *it);
	}
}




void
c8021core::hook_port_up(const rofl::crofdpt& dpt, std::string const& devname)
{
	try {

		std::vector<std::string> argv;
		std::vector<std::string> envp;

		std::stringstream s_dpid;
		s_dpid << "DPID=" << dpt.get_dpid();
		envp.push_back(s_dpid.str());

		std::stringstream s_devname;
		s_devname << "DEVNAME=" << devname;
		envp.push_back(s_devname.str());

		c8021core::execute(c8021core::script_path_port_up, argv, envp);

	} catch (rofl::eRofDptNotFound& e) {
		rofcore::logging::error << "[cbasebox][hook_port_up] script execution failed" << std::endl;
	}
}



void
c8021core::hook_port_down(const rofl::crofdpt& dpt, std::string const& devname)
{
	try {

		std::vector<std::string> argv;
		std::vector<std::string> envp;

		std::stringstream s_dpid;
		s_dpid << "DPID=" << dpt.get_dpid();
		envp.push_back(s_dpid.str());

		std::stringstream s_devname;
		s_devname << "DEVNAME=" << devname;
		envp.push_back(s_devname.str());

		c8021core::execute(c8021core::script_path_port_down, argv, envp);

	} catch (rofl::eRofDptNotFound& e) {
		rofcore::logging::error << "[cbasebox][hook_port_down] script execution failed" << std::endl;
	}
}



void
c8021core::execute(
		std::string const& executable,
		std::vector<std::string> argv,
		std::vector<std::string> envp)
{
	pid_t pid = 0;

	if ((pid = fork()) < 0) {
		rofcore::logging::error << "[cbasebox][execute] syscall error fork(): " << errno << ":" << strerror(errno) << std::endl;
		return;
	}

	if (pid > 0) { // father process
		int status;
		waitpid(pid, &status, 0);
		return;
	}


	// child process

	std::vector<const char*> vctargv;
	for (std::vector<std::string>::iterator
			it = argv.begin(); it != argv.end(); ++it) {
		vctargv.push_back((*it).c_str());
	}
	vctargv.push_back(NULL);


	std::vector<const char*> vctenvp;
	for (std::vector<std::string>::iterator
			it = envp.begin(); it != envp.end(); ++it) {
		vctenvp.push_back((*it).c_str());
	}
	vctenvp.push_back(NULL);


	execvpe(executable.c_str(), (char*const*)&vctargv[0], (char*const*)&vctenvp[0]);

	exit(1); // just in case execvpe fails
}

