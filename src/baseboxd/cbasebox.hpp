/*
 * cbasebox.hpp
 *
 *  Created on: 05.08.2014
 *      Author: andreas
 */

#ifndef CROFBASE_HPP_
#define CROFBASE_HPP_

#include <iostream>
#include <exception>
#include <rofl/common/crofbase.h>

#include <rofl/platform/unix/cdaemon.h>
#include <rofl/platform/unix/cunixenv.h>

#include "cethcore.hpp"
#include "cipcore.hpp"
#include "cgtpcore.hpp"
#include "cgrecore.hpp"
#include "clogging.hpp"
#include "cconfig.hpp"
#include "cnetlink.hpp"
#include "cgtprelay.hpp"
#include "ctundev.hpp"

namespace basebox {

class eBaseBoxBase : public std::runtime_error {
public:
	eBaseBoxBase(const std::string& __arg) : std::runtime_error(__arg) {};
};
class eLinkNoDptAttached		: public eBaseBoxBase {
public:
	eLinkNoDptAttached(const std::string& __arg) : eBaseBoxBase(__arg) {};
};
class eLinkTapDevNotFound		: public eBaseBoxBase {
public:
	eLinkTapDevNotFound(const std::string& __arg) : eBaseBoxBase(__arg) {};
};

class cbasebox : public rofl::crofbase, public rofcore::cnetdev_owner, public rofcore::cnetlink_common_observer {

	/**
	 * @brief	pointer to singleton
	 */
	static cbasebox*	rofbase;

	/**
	 *
	 */
	cbasebox(
			const rofl::openflow::cofhello_elem_versionbitmap& versionbitmap =
					rofl::openflow::cofhello_elem_versionbitmap()) :
						rofl::crofbase(versionbitmap) {};

	/**
	 *
	 */
	virtual
	~cbasebox() {};

	/**
	 *
	 */
	cbasebox(
			const cbasebox& ethbase);

public:

	/**
	 *
	 */
	static cbasebox&
	get_instance(
			const rofl::openflow::cofhello_elem_versionbitmap& versionbitmap =
					rofl::openflow::cofhello_elem_versionbitmap()) {
		if (cbasebox::rofbase == (cbasebox*)0) {
			cbasebox::rofbase = new cbasebox(versionbitmap);
		}
		return *(cbasebox::rofbase);
	};

	/**
	 *
	 */
	static int
	run(int argc, char** argv);

protected:

	/**
	 *
	 */
	virtual void
	handle_dpt_open(
			rofl::crofdpt& dpt);

	/**
	 *
	 */
	virtual void
	handle_dpt_close(
			rofl::crofdpt& dpt);

	/**
	 *
	 */
	virtual void
	handle_packet_in(
			rofl::crofdpt& dpt, const rofl::cauxid& auxid, rofl::openflow::cofmsg_packet_in& msg);

	/**
	 *
	 */
	virtual void
	handle_flow_removed(
			rofl::crofdpt& dpt, const rofl::cauxid& auxid, rofl::openflow::cofmsg_flow_removed& msg);

	/**
	 *
	 */
	virtual void
	handle_port_status(
			rofl::crofdpt& dpt, const rofl::cauxid& auxid, rofl::openflow::cofmsg_port_status& msg);

	/**
	 *
	 */
	virtual void
	handle_error_message(
			rofl::crofdpt& dpt, const rofl::cauxid& auxid, rofl::openflow::cofmsg_error& msg);

	/**
	 *
	 */
	virtual void
	handle_port_desc_stats_reply(
			rofl::crofdpt& dpt, const rofl::cauxid& auxid, rofl::openflow::cofmsg_port_desc_stats_reply& msg);

	/**
	 *
	 */
	virtual void
	handle_port_desc_stats_reply_timeout(rofl::crofdpt& dpt, uint32_t xid);

public:

	/*
	 * from ctapdev
	 */
	virtual void
	enqueue(
			rofcore::cnetdev *netdev, rofl::cpacket* pkt);

	virtual void
	enqueue(
			rofcore::cnetdev *netdev, std::vector<rofl::cpacket*> pkts);



private:

	/**
	 *
	 */
	rofcore::ctapdev&
	add_tap_dev(uint32_t ofp_port_no, const std::string& devname, const rofl::caddress_ll& hwaddr) {
		if (devs.find(ofp_port_no) != devs.end()) {
			delete devs[ofp_port_no];
		}
		devs[ofp_port_no] = new rofcore::ctapdev(this, devname, hwaddr, ofp_port_no);
		return *(devs[ofp_port_no]);
	};

	/**
	 *
	 */
	rofcore::ctapdev&
	set_tap_dev(uint32_t ofp_port_no, const std::string& devname, const rofl::caddress_ll& hwaddr) {
		if (devs.find(ofp_port_no) == devs.end()) {
			devs[ofp_port_no] = new rofcore::ctapdev(this, devname, hwaddr, ofp_port_no);
		}
		return *(devs[ofp_port_no]);
	};

	/**
	 *
	 */
	rofcore::ctapdev&
	set_tap_dev(uint32_t ofp_port_no) {
		if (devs.find(ofp_port_no) == devs.end()) {
			throw rofcore::eTunDevNotFound();
		}
		return *(devs[ofp_port_no]);
	};

	/**
	 *
	 */
	rofcore::ctapdev&
	set_tap_dev(const rofl::caddress_ll& hwaddr) {
		std::map<uint32_t, rofcore::ctapdev*>::iterator it;
		if ((it = find_if(devs.begin(), devs.end(),
				rofcore::ctapdev::ctapdev_find_by_hwaddr(hwaddr))) == devs.end()) {
			throw rofcore::eTunDevNotFound();
		}
		return *(it->second);
	};

	/**
	 *
	 */
	rofcore::ctapdev&
	set_tap_dev(const std::string& devname) {
		std::map<uint32_t, rofcore::ctapdev*>::iterator it;
		if ((it = find_if(devs.begin(), devs.end(),
				rofcore::ctapdev::ctapdev_find_by_devname(devname))) == devs.end()) {
			throw rofcore::eTunDevNotFound();
		}
		return *(it->second);
	};

	/**
	 *
	 */
	const rofcore::ctapdev&
	get_tap_dev(uint32_t ofp_port_no) const {
		if (devs.find(ofp_port_no) == devs.end()) {
			throw rofcore::eTunDevNotFound();
		}
		return *(devs.at(ofp_port_no));
	};

	/**
	 *
	 */
	const rofcore::ctapdev&
	get_tap_dev(const rofl::caddress_ll& hwaddr) const {
		std::map<uint32_t, rofcore::ctapdev*>::const_iterator it;
		if ((it = find_if(devs.begin(), devs.end(),
				rofcore::ctapdev::ctapdev_find_by_hwaddr(hwaddr))) == devs.end()) {
			throw rofcore::eTunDevNotFound();
		}
		return *(it->second);
	};

	/**
	 *
	 */
	const rofcore::ctapdev&
	get_tap_dev(const std::string& devname) const {
		std::map<uint32_t, rofcore::ctapdev*>::const_iterator it;
		if ((it = find_if(devs.begin(), devs.end(),
				rofcore::ctapdev::ctapdev_find_by_devname(devname))) == devs.end()) {
			throw rofcore::eTunDevNotFound();
		}
		return *(it->second);
	};

	/**
	 *
	 */
	void
	drop_tap_dev(uint32_t ofp_port_no) {
		if (devs.find(ofp_port_no) == devs.end()) {
			return;
		}
		delete devs[ofp_port_no];
		devs.erase(ofp_port_no);
	};

	/**
	 *
	 */
	void
	drop_tap_dev(const rofl::caddress_ll& hwaddr) {
		std::map<uint32_t, rofcore::ctapdev*>::const_iterator it;
		if ((it = find_if(devs.begin(), devs.end(),
				rofcore::ctapdev::ctapdev_find_by_hwaddr(hwaddr))) == devs.end()) {
			return;
		}
		delete it->second;
		devs.erase(it->first);
	};

	/**
	 *
	 */
	void
	drop_tap_dev(const std::string& devname) {
		std::map<uint32_t, rofcore::ctapdev*>::const_iterator it;
		if ((it = find_if(devs.begin(), devs.end(),
				rofcore::ctapdev::ctapdev_find_by_devname(devname))) == devs.end()) {
			return;
		}
		delete it->second;
		devs.erase(it->first);
	};

	/**
	 *
	 */
	bool
	has_tap_dev(uint32_t ofp_port_no) const {
		return (not (devs.find(ofp_port_no) == devs.end()));
	};

	/**
	 *
	 */
	bool
	has_tap_dev(const rofl::caddress_ll& hwaddr) const {
		std::map<uint32_t, rofcore::ctapdev*>::const_iterator it;
		if ((it = find_if(devs.begin(), devs.end(),
				rofcore::ctapdev::ctapdev_find_by_hwaddr(hwaddr))) == devs.end()) {
			return false;
		}
		return true;
	};

	/**
	 *
	 */
	bool
	has_tap_dev(const std::string& devname) const {
		std::map<uint32_t, rofcore::ctapdev*>::const_iterator it;
		if ((it = find_if(devs.begin(), devs.end(),
				rofcore::ctapdev::ctapdev_find_by_devname(devname))) == devs.end()) {
			return false;
		}
		return true;
	};

public:

	friend std::ostream&
	operator<< (std::ostream& os, const cbasebox& box) {
		os << rofcore::indent(0) << "<cbasebox dptid: " << box.dptid << " >" << std::endl;
		rofcore::indent i(2);
		for (std::map<uint32_t, rofcore::ctapdev*>::const_iterator
				it = box.devs.begin(); it != box.devs.end(); ++it) {
			os << *(it->second);
		}
		os << rofip::cipcore::get_ip_core(rofl::crofdpt::get_dpt(box.dptid).get_dpid());
		try {
			os << rofeth::cethcore::get_eth_core(rofl::crofdpt::get_dpt(box.dptid).get_dpid());
		} catch (rofl::eRofDptNotFound& e) {}
		return os;
	};

private:

	friend class cnetlink;

	virtual void
	addr_in4_created(unsigned int ifindex, uint16_t adindex);

	virtual void
	addr_in4_updated(unsigned int ifindex, uint16_t adindex) {};

	virtual void
	addr_in4_deleted(unsigned int ifindex, uint16_t adindex);

	virtual void
	addr_in6_created(unsigned int ifindex, uint16_t adindex);

	virtual void
	addr_in6_updated(unsigned int ifindex, uint16_t adindex) {};

	virtual void
	addr_in6_deleted(unsigned int ifindex, uint16_t adindex);

private:

	/*
	 * event specific hooks
	 */
	void
	hook_dpt_attach();

	void
	hook_dpt_detach();

	void
	hook_port_up(std::string const& devname);

	void
	hook_port_down(std::string const& devname);

	void
	set_forwarding(bool forward = true);

	static void
	execute(
			std::string const& executable,
			std::vector<std::string> argv,
			std::vector<std::string> envp);

private:

	/**
	 *
	 */
	void
	test_workflow();

private:

	rofl::cdptid dptid;
	static const std::string BASEBOX_LOG_FILE;
	static const std::string BASEBOX_PID_FILE;
	static const std::string BASEBOX_CONFIG_FILE;
	static const std::string BASEBOX_CONFIG_DPT_LIST;

	static std::string script_path_dpt_open;
	static std::string script_path_dpt_close;
	static std::string script_path_port_up;
	static std::string script_path_port_down;

	std::map<uint32_t, rofcore::ctapdev*>	devs;
};

}; // end of namespace ethcore

#endif /* CROFBASE_HPP_ */
