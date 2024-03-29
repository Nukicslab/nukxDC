/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2015 Software Radio Systems Limited
 *
 * \section LICENSE
 *
 * This file is part of the srsUE library.
 *
 * srsUE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsUE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */


#include "srslte/upper/pdcp.h"

namespace srslte {

pdcp::pdcp()
{
  rlc = NULL;
  rrc = NULL;
  gw = NULL;
  pdcp_log = NULL;
  lcid = 0;
  direction = 0;
}

void pdcp::init(srsue::rlc_interface_pdcp *rlc_, srsue::lwaap_interface_pdcp *lwaap_, srsue::rrc_interface_pdcp *rrc_, srsue::gw_interface_pdcp *gw_, log *pdcp_log_, uint32_t lcid_, uint8_t direction_)
{
  rlc       = rlc_;
  lwaap     = lwaap_;
  rrc       = rrc_;
  gw        = gw_;
  pdcp_log  = pdcp_log_;
  lcid      = lcid_;
  direction = direction_;

  // Default config
  srslte_pdcp_config_t cnfg;
  cnfg.is_control = false;
  cnfg.is_data = false;
  cnfg.direction = direction_;

  pdcp_array[0].init(rlc, lwaap, rrc, gw, pdcp_log, lcid, cnfg);
}

void pdcp::stop()
{
}

// Metric interface
void pdcp::get_metrics(pdcp_metrics_t &m)
{
  // Current only lcid 3
  pdcp_array[3].get_metrics(m);
}


void pdcp::reestablish() {
  for(uint32_t i=0;i<SRSLTE_N_RADIO_BEARERS;i++) {
    if (pdcp_array[i].is_active()) {
      pdcp_array[i].reestablish();
    }
  }
}

void pdcp::reset()
{
  for(uint32_t i=0;i<SRSLTE_N_RADIO_BEARERS;i++) {
    pdcp_array[i].reset();
  }

  pdcp_array[0].init(rlc, lwaap, rrc, gw, pdcp_log, lcid, direction);
}

/*******************************************************************************
  RRC/GW interface
*******************************************************************************/
bool pdcp::is_drb_enabled(uint32_t lcid)
{
  if(lcid >= SRSLTE_N_RADIO_BEARERS) {
    pdcp_log->error("Radio bearer id must be in [0:%d] - %d\n", SRSLTE_N_RADIO_BEARERS, lcid);
    return false;
  }
  return pdcp_array[lcid].is_active();
}

void pdcp::write_sdu(uint32_t lcid, byte_buffer_t *sdu)
{
  if(valid_lcid(lcid)) {
    pdcp_array[lcid].write_sdu(sdu);
  } else {
    pdcp_log->warning("Writing sdu: lcid=%d. Deallocating sdu\n", lcid);
    byte_buffer_pool::get_instance()->deallocate(sdu);
  }
}

void pdcp::write_sdu_mch(uint32_t lcid, byte_buffer_t *sdu)
{
  if(valid_mch_lcid(lcid)){
    pdcp_array_mrb[lcid].write_sdu(sdu);
  }
}
void pdcp::add_bearer(uint32_t lcid, srslte_pdcp_config_t cfg)
{
  if(lcid >= SRSLTE_N_RADIO_BEARERS) {
    pdcp_log->error("Radio bearer id must be in [0:%d] - %d\n", SRSLTE_N_RADIO_BEARERS, lcid);
    return;
  }
  if (!pdcp_array[lcid].is_active()) {
    pdcp_array[lcid].init(rlc, lwaap, rrc, gw, pdcp_log, lcid, cfg);
    pdcp_log->info("Added bearer %s\n", rrc->get_rb_name(lcid).c_str());
  } else {
    pdcp_log->warning("Bearer %s already configured. Reconfiguration not supported\n", rrc->get_rb_name(lcid).c_str());
  }
}


void pdcp::add_bearer_mrb(uint32_t lcid, srslte_pdcp_config_t cfg)
{
  if(lcid >= SRSLTE_N_RADIO_BEARERS) {
    pdcp_log->error("Radio bearer id must be in [0:%d] - %d\n", SRSLTE_N_RADIO_BEARERS, lcid);
    return;
  }
  if (!pdcp_array_mrb[lcid].is_active()) {
    pdcp_array_mrb[lcid].init(rlc, lwaap, rrc, gw, pdcp_log, lcid, cfg);
    pdcp_log->info("Added bearer %s\n", rrc->get_rb_name(lcid).c_str());
  } else {
    pdcp_log->warning("Bearer %s already configured. Reconfiguration not supported\n", rrc->get_rb_name(lcid).c_str());
  }
}

void pdcp::config_security(uint32_t lcid,
                           uint8_t *k_enc,
                           uint8_t *k_int,
                           CIPHERING_ALGORITHM_ID_ENUM cipher_algo,
                           INTEGRITY_ALGORITHM_ID_ENUM integ_algo)
{
  if(valid_lcid(lcid))
    pdcp_array[lcid].config_security(k_enc, k_int, cipher_algo, integ_algo);
}

void pdcp::config_security_all(uint8_t *k_enc,
                               uint8_t *k_int,
                               CIPHERING_ALGORITHM_ID_ENUM cipher_algo,
                               INTEGRITY_ALGORITHM_ID_ENUM integ_algo)
{
  for(uint32_t i=0;i<SRSLTE_N_RADIO_BEARERS;i++) {
    if (pdcp_array[i].is_active()) {
      pdcp_array[i].config_security(k_enc, k_int, cipher_algo, integ_algo);
    }
  }
}

void pdcp::enable_integrity(uint32_t lcid)
{
  if(valid_lcid(lcid))
    pdcp_array[lcid].enable_integrity();
}

void pdcp::enable_encryption(uint32_t lcid)
{
  if(valid_lcid(lcid))
    pdcp_array[lcid].enable_encryption();
}

/*******************************************************************************
  RLC interface
*******************************************************************************/
void pdcp::write_pdu(uint32_t lcid, byte_buffer_t *pdu)
{
  if(valid_lcid(lcid)) {
    pdcp_array[lcid].write_pdu(pdu);
  } else {
    pdcp_log->warning("Writing pdu: lcid=%d. Deallocating pdu\n", lcid);
    byte_buffer_pool::get_instance()->deallocate(pdu);
  }
}

void pdcp::write_pdu_bcch_bch(byte_buffer_t *sdu)
{
  rrc->write_pdu_bcch_bch(sdu);
}
void pdcp::write_pdu_bcch_dlsch(byte_buffer_t *sdu)
{
  rrc->write_pdu_bcch_dlsch(sdu);
}

void pdcp::write_pdu_pcch(byte_buffer_t *sdu)
{
  rrc->write_pdu_pcch(sdu);
}

void pdcp::write_pdu_mch(uint32_t lcid, byte_buffer_t *sdu)
{
  if(0 == lcid) {
    rrc->write_pdu_mch(lcid, sdu);
  } else {
    gw->write_pdu_mch(lcid, sdu);
  }
}

/*******************************************************************************
  Helpers
*******************************************************************************/
bool pdcp::valid_lcid(uint32_t lcid)
{
  if(lcid >= SRSLTE_N_RADIO_BEARERS) {
    pdcp_log->error("Radio bearer id must be in [0:%d] - %d", SRSLTE_N_RADIO_BEARERS, lcid);
    return false;
  }
  if(!pdcp_array[lcid].is_active()) {
    pdcp_log->error("PDCP entity for logical channel %d has not been activated\n", lcid);
    return false;
  }
  return true;
}

void pdcp::set_lwa_ratio(uint32_t lr, uint32_t wr)
{
  // Current only lcid 3
  pdcp_array[3].set_lwa_ratio(lr, wr);
}

void pdcp::set_ema_ratio(uint32_t part, uint32_t whole)
{
  // Current only lcid 3
  pdcp_array[3].set_ema_ratio(part, whole);
}

void pdcp::set_report_period(uint32_t period)
{
  // Current only lcid 3
  pdcp_array[3].set_report_period(period);
}

void pdcp::toggle_timestamp(bool b)
{
  // Current only DRB
  pdcp_array[3].toggle_timestamp(b);
}

void pdcp::toggle_autoconfig(bool b)
{
  // Current only DRB
  pdcp_array[3].toggle_autoconfig(b);
}

void pdcp::toggle_random(bool b)
{
  // Current only DRB
  pdcp_array[3].toggle_random(b);
}

bool pdcp::valid_mch_lcid(uint32_t lcid)
{
  if(lcid >= SRSLTE_N_MCH_LCIDS) {
    pdcp_log->error("Radio bearer id must be in [0:%d] - %d", SRSLTE_N_RADIO_BEARERS, lcid);
    return false;
  }
  if(!pdcp_array_mrb[lcid].is_active()) {
    pdcp_log->error("PDCP entity for logical channel %d has not been activated\n", lcid);
    return false;
  }
  return true;
}

} // namespace srsue
