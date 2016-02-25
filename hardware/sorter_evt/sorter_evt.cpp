/*******************************************************************************
 *
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 *
 * This software can be redistributed and/or modified freely provided that
 * any derivative works bear some notice that they are derived from it, and
 * any modified versions bear some notice that they have been modified.
 *
 * Author(s):
 *      Martin Shetty (NIST)
 *
 * Description:
 *      Qpx::SorterEVT
 *
 ******************************************************************************/

#include "sorter_evt.h"
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include "custom_logger.h"
#include "custom_timer.h"

#include "URL.h"
#include "CRingItem.h"
#include "DataFormat.h"
#include "CRingItemFactory.h"

#include "CPhysicsEventItem.h"
#include "CDataFormatItem.h"
#include "CRingStateChangeItem.h"
#include "CRingPhysicsEventCountItem.h"

#include "qpx_util.h"

#include "MADC32_module.h"

namespace Qpx {

static DeviceRegistrar<SorterEVT> registrar("SorterEVT");

SorterEVT::SorterEVT() {
  status_ = DeviceStatus::loaded | DeviceStatus::can_boot;

  runner_ = nullptr;

  run_status_.store(0);
  expected_rbuf_items_ = 0;

  loop_data_ = false;
  override_timestamps_= false;
  bad_buffers_rep_ = false;
  bad_buffers_dbg_ = false;
  terminate_premature_ = false;
  max_rbuf_evts_ = 0;
}

bool SorterEVT::die() {
  files_.clear();
  expected_rbuf_items_ = 0;
  status_ = DeviceStatus::loaded | DeviceStatus::can_boot;
//  for (auto &q : set.branches.my_data_) {
//    if ((q.metadata.setting_type == Gamma::SettingType::file_path) && (q.id_ == "SorterEVT/Source file"))
//      q.metadata.writable = true;
//  }

  return true;
}


SorterEVT::~SorterEVT() {
  daq_stop();
  if (runner_ != nullptr) {
    runner_->detach();
    delete runner_;
  }
  die();
}

bool SorterEVT::daq_start(SynchronizedQueue<Spill*>* out_queue) {
  if (run_status_.load() > 0)
    return false;

  run_status_.store(1);

  if (runner_ != nullptr)
    delete runner_;

  runner_ = new boost::thread(&worker_run, this, out_queue);

  return true;
}

bool SorterEVT::daq_stop() {
  if (run_status_.load() == 0)
    return false;

  run_status_.store(2);

  if ((runner_ != nullptr) && runner_->joinable()) {
    runner_->join();
    delete runner_;
    runner_ = nullptr;
  }

  wait_ms(500);

  run_status_.store(0);
  return true;
}

bool SorterEVT::daq_running() {
  if (run_status_.load() == 3)
    daq_stop();
  return (run_status_.load() > 0);
}



bool SorterEVT::read_settings_bulk(Gamma::Setting &set) const {
  if (set.id_ == device_name()) {
    for (auto &q : set.branches.my_data_) {
      if ((q.metadata.setting_type == Gamma::SettingType::boolean) && (q.id_ == "SorterEVT/Override timestamps"))
        q.value_int = override_timestamps_;
      else if ((q.metadata.setting_type == Gamma::SettingType::boolean) && (q.id_ == "SorterEVT/Loop data"))
        q.value_int = loop_data_;
      else if ((q.metadata.setting_type == Gamma::SettingType::boolean) && (q.id_ == "SorterEVT/Override pause"))
        q.value_int = override_pause_;
      else if ((q.metadata.setting_type == Gamma::SettingType::boolean) && (q.id_ == "SorterEVT/Bad_buffers_report"))
        q.value_int = bad_buffers_rep_;
      else if ((q.metadata.setting_type == Gamma::SettingType::boolean) && (q.id_ == "SorterEVT/Bad_buffers_output"))
        q.value_int = bad_buffers_dbg_;
      else if ((q.metadata.setting_type == Gamma::SettingType::integer) && (q.id_ == "SorterEVT/Pause"))
        q.value_int = pause_ms_;
      else if ((q.metadata.setting_type == Gamma::SettingType::boolean) && (q.id_ == "SorterEVT/Cutoff"))
        q.value_int = terminate_premature_;
      else if ((q.metadata.setting_type == Gamma::SettingType::boolean) && (q.id_ == "SorterEVT/Cutoff number"))
        q.value_int = max_rbuf_evts_;
      else if ((q.metadata.setting_type == Gamma::SettingType::dir_path) && (q.id_ == "SorterEVT/Source dir")) {
        q.value_text = source_dir_;
        q.metadata.writable = !(status_ & DeviceStatus::booted);
      }
    }
  }
  return true;
}


bool SorterEVT::write_settings_bulk(Gamma::Setting &set) {
  set.enrich(setting_definitions_);

  if (set.id_ != device_name())
    return false;

  for (auto &q : set.branches.my_data_) {
    if (q.id_ == "SorterEVT/Override timestamps")
      override_timestamps_ = q.value_int;
    else if (q.id_ == "SorterEVT/Loop data")
      loop_data_ = q.value_int;
    else if (q.id_ == "SorterEVT/Override pause")
      override_pause_ = q.value_int;
    else if (q.id_ == "SorterEVT/Bad_buffers_report")
      bad_buffers_rep_ = q.value_int;
    else if (q.id_ == "SorterEVT/Bad_buffers_output")
      bad_buffers_dbg_ = q.value_int;
    else if (q.id_ == "SorterEVT/Pause")
      pause_ms_ = q.value_int;
    else if (q.id_ == "SorterEVT/Cutoff")
      terminate_premature_ = q.value_int;
    else if (q.id_ == "SorterEVT/Cutoff number")
      max_rbuf_evts_ = q.value_int;
    else if (q.id_ == "SorterEVT/Source dir")
      source_dir_ = q.value_text;
  }
  return true;
}

bool SorterEVT::boot() {
  if (!(status_ & DeviceStatus::can_boot)) {
    PL_WARN << "<SorterEVT> Cannot boot Sorter. Failed flag check (can_boot == 0)";
    return false;
  }

  status_ = DeviceStatus::loaded | DeviceStatus::can_boot;

  files_.clear();
  expected_rbuf_items_ = 0;

  namespace fs = boost::filesystem;

  fs::path manifest_file(source_dir_);
  manifest_file /= "evt_manifest.xml";

  if (fs::is_regular_file(manifest_file)) {
    pugi::xml_document doc;
    if (doc.load_file(manifest_file.string().c_str())) {
      pugi::xml_node root = doc.first_child();
      if (root && (std::string(root.name()) == "EvtManifest")) {
        for (pugi::xml_node child : root.children()) {
          std::string name = std::string(child.name());
          if (name == "File") {
            std::string filename(child.attribute("path").value());
            PL_INFO << "<SorterEVT> Queued up file " << filename << " from XML manifest";
            files_.push_back(filename);
          } else if (name == "Total") {
            expected_rbuf_items_ = child.attribute("RingBufferItems").as_ullong();
          }
        }
      }
    }
  }

  if (files_.empty() || (expected_rbuf_items_ == 0)) {

    fs::path someDir(source_dir_);
    fs::directory_iterator end_iter;
    std::set<std::string> files_prelim;

    if ( fs::exists(someDir) && fs::is_directory(someDir))
    {
      for( fs::directory_iterator dir_iter(someDir); dir_iter != end_iter; ++dir_iter)
      {
        if ( fs::is_regular_file(dir_iter->status()) )
        {
          //        PL_DBG << "Looking at  " << dir_iter->path().extension().string();
          if (boost::algorithm::to_lower_copy(dir_iter->path().extension().string()) == ".evt") {
            //        PL_DBG << "File is evt:  " << dir_iter->path().string();
            files_prelim.insert(dir_iter->path().string());
          }
        }
      }
    }

    CFileDataSource* cfds;
    expected_rbuf_items_ = 0;
    for (auto &q : files_prelim) {
      //    PL_DBG << "checking " << q;
      uint64_t cts = 0;
      if (((cfds = open_EVT_file(q)) != nullptr) && (cts = num_of_evts(cfds))) {
        delete cfds;
        files_.push_back(q);
        PL_INFO << "<SorterEVT> Queued up file " << q << " with " << cts << " ring buffer items";
        expected_rbuf_items_ += cts;
      }
    }

    if (files_.empty())
      return false;

    pugi::xml_document xml_doc;
    pugi::xml_node xml_root = xml_doc.append_child("EvtManifest");
    for (auto &q : files_) {
      xml_root.append_child("File");
      xml_root.last_child().append_attribute("path").set_value(q.c_str());
    }
    xml_root.append_child("Total");
    xml_root.last_child().append_attribute("RingBufferItems").set_value(std::to_string(expected_rbuf_items_).c_str());
    xml_doc.save_file(manifest_file.string().c_str());
  }


  PL_INFO << "<SorterEVT> successfully queued up EVT files for sorting with "
          << expected_rbuf_items_ << " total ring buffer items";

  status_ = DeviceStatus::loaded | DeviceStatus::booted | DeviceStatus::can_run;
//  for (auto &q : set.branches.my_data_) {
//    if ((q.metadata.setting_type == Gamma::SettingType::file_path) && (q.id_ == "SorterEVT/Source file"))
//      q.metadata.writable = false;
//  }

  return true;
}

uint64_t SorterEVT::num_of_evts(CFileDataSource *evtfile) {
  if (evtfile == nullptr)
    return 0;

  uint64_t count = 0;
  CRingItem* item;
  while ((item = evtfile->getItem()) != NULL)  {
    count++;
    delete item;
  }

  return count;
}


CFileDataSource* SorterEVT::open_EVT_file(std::string file) {
  std::string filename("file://");
  filename += file;

  try { URL url(filename); }
  catch (...) { PL_ERR << "<SorterEVT> could not parse URL"; return nullptr; }

  URL url(filename);

  CFileDataSource* evtfile;
  try { evtfile = new CFileDataSource(url, std::vector<uint16_t>()); }
  catch (...) { PL_ERR << "<SorterEVT> could not open EVT file"; return nullptr; }

  return evtfile;
}


void SorterEVT::get_all_settings() {
  if (status_ & DeviceStatus::booted) {
  }
}


void SorterEVT::worker_run(SorterEVT* callback, SynchronizedQueue<Spill*>* spill_queue) {
  PL_DBG << "<SorterEVT> Start run worker";

  Spill one_spill;
  bool timeout = false;
  std::set<int> starts_signalled;

  uint64_t count = 0;
  uint64_t events = 0;
  uint64_t lost_events = 0;
  uint64_t last_time = 0;

  CFileDataSource* evt_file = nullptr;
  CRingItem* item = nullptr;
  CRingItemFactory  fact;

  boost::posix_time::ptime time_start;
  boost::posix_time::ptime ts;

  int filenr = 0;

  for (auto &file : callback->files_) {
    if (timeout)
      break;

    PL_DBG << "<SorterEVT> Now processing " << file;

    evt_file = open_EVT_file(file);
    if (evt_file == nullptr) {
      PL_ERR << "<SorterEVT> Could not open " << file << ". Aborting.";
      break;
    }

    filenr ++;

    while (!timeout) {

    one_spill = Spill();

    bool done = false;
    std::list<uint32_t> prev_MADC_data;
    std::string prev_pattern;

    while ( (!callback->terminate_premature_ || (count < callback->max_rbuf_evts_)) && (!done) && (item = evt_file->getItem()) != NULL)  {
      count++;

      switch (item->type()) {

      case RING_FORMAT: {
        CDataFormatItem* pEvent = reinterpret_cast<CDataFormatItem*>(fact.createRingItem(*item));
        if (pEvent) {
          //PL_DBG << "Ring format: " << pEvent->toString();
          delete pEvent;
        }
        break;
      }
      case END_RUN:
      case BEGIN_RUN:
      {
        CRingStateChangeItem* pEvent = reinterpret_cast<CRingStateChangeItem*>(fact.createRingItem(*item));
        if (pEvent) {

          ts = boost::posix_time::from_time_t(pEvent->getTimestamp());
  //        PL_DBG << "State :" << pEvent->toString();
          PL_DBG << "State  ts=" << boost::posix_time::to_iso_extended_string(ts)
                 << "  elapsed=" << pEvent->getElapsedTime()
                 << "  run#=" << pEvent->getRunNumber()
                 << "  barrier=" << pEvent->getBarrierType();

          if (pEvent->getBarrierType() == 1) {
            time_start = ts;
            starts_signalled.clear();
          } else if (pEvent->getBarrierType() == 2) {
            done = true;
          }

          delete pEvent;
        }
        break;
      }

      case PHYSICS_EVENT_COUNT:
      {
        CRingPhysicsEventCountItem* pEvent = reinterpret_cast<CRingPhysicsEventCountItem*>(fact.createRingItem(*item));
        if (pEvent) {
  //        PL_DBG << "Physics counts: " << pEvent->toString();
          ts = boost::posix_time::from_time_t(pEvent->getTimestamp());
//          PL_DBG << "State  ts=" << boost::posix_time::to_iso_extended_string(ts)
//                 << "  elapsed=" << pEvent->getTimeOffset()
//                 << "  total_events=" << pEvent->getEventCount();

          for (auto &q : starts_signalled) {
            StatsUpdate udt;
            udt.model_hit = MADC32::model_hit();

            udt.source_channel = q;
            udt.lab_time = ts;
            udt.fast_peaks = pEvent->getEventCount();
            udt.live_time = pEvent->getTimeOffset();
            udt.total_time = pEvent->getTimeOffset();

//            PL_DBG << "<SorterEVT> " << udt.to_string();

            one_spill.stats.push_back(udt);
            done = true;
          }

          delete pEvent;
        }
        break;
      }


      case PHYSICS_EVENT: {
        CPhysicsEventItem* pEvent = reinterpret_cast<CPhysicsEventItem*>(fact.createRingItem(*item));
        if (pEvent) {

            uint32_t  bytes = pEvent->getBodySize();
            uint32_t  words = bytes/sizeof(uint16_t);

            const uint16_t* body  = reinterpret_cast<const uint16_t*>((const_cast<CPhysicsEventItem*>(pEvent))->getBodyPointer());
            uint16_t expected_words = *body;
            if (expected_words == (words - 1)) {
              //PL_DBG << "Header indicates " << expected_words << " expected 16-bit words. Correct";
              body++;

              std::list<uint32_t> MADC_data;
              for (int i=0; i < expected_words; i+=2) {
                uint32_t lower = *body++;
                uint32_t upper = *body++;
                MADC_data.push_back(lower | (upper << 16));
              }

              std::string madc_pattern;
              std::list<Hit> hits = Qpx::MADC32::parse(MADC_data, events, last_time, madc_pattern);

              for (auto &h : hits) {
                if (!starts_signalled.count(h.source_channel)) {
                  StatsUpdate udt;
                  udt.model_hit = MADC32::model_hit();

                  udt.source_channel = h.source_channel;
                  udt.lab_time = time_start;
                  udt.stats_type = StatsType::start;
                  //udt.fast_peaks = pEvent->getEventCount();
                  //udt.live_time = pEvent->getTimeOffset();
                  //udt.real_time = pEvent->getTimeOffset();
                  Spill extra_spill;
                  extra_spill.stats.push_back(udt);
                  spill_queue->enqueue(new Spill(extra_spill));
                  starts_signalled.insert(h.source_channel);
                }
              }


              bool buffer_problem = false;

              size_t n_h = std::count(madc_pattern.begin(), madc_pattern.end(), 'H');
              size_t n_e = std::count(madc_pattern.begin(), madc_pattern.end(), 'E');
              size_t n_f = std::count(madc_pattern.begin(), madc_pattern.end(), 'F');
              size_t n_j = std::count(madc_pattern.begin(), madc_pattern.end(), 'J');

              if (n_j > 1) {
                if (callback->bad_buffers_rep_)
                  PL_DBG << "<SorterEVT> MADC32 parse has multiple junk words, pattern: " << madc_pattern << " after previous " << prev_pattern;
                buffer_problem = true;
              }
              if (n_e != hits.size()) {
                if (callback->bad_buffers_rep_)
                  PL_DBG << "<SorterEVT> MADC32 parse has mismatch in number of retrieved events, pattern: " << madc_pattern << " after previous " << prev_pattern;
                buffer_problem = true;
                lost_events += n_e;
              }
              if (n_h != n_f) {
                if (callback->bad_buffers_rep_)
                  PL_DBG << "<SorterEVT> MADC32 parse has mismatch in header and footer, pattern: " << madc_pattern << " after previous " << prev_pattern;
                buffer_problem = true;
              }

              if (callback->bad_buffers_dbg_ && buffer_problem) {
                PL_DBG << "  " << buffer_to_string(MADC_data);
              }



              one_spill.hits.splice(one_spill.hits.end(), hits);

              prev_MADC_data = MADC_data;
              prev_pattern = madc_pattern;


            } else
              PL_DBG << "<SorterEVT> Header indicates " << expected_words << " expected 16-bit words, but does not match body size = " << (words - 1);

          delete pEvent;
        }
        break;
      }

      default: {
        PL_DBG << "<SorterEVT> Unexpected ring buffer item type " << item->type();
      }

      }

      delete item;
    }

    PL_DBG << "<SorterEVT> Processed [" << filenr << "/" << callback->files_.size() << "] "
           << (100.0 * count / callback->expected_rbuf_items_) << "%  cumulative hits = " << events
           << "   hits lost in bad buffers = " << lost_events
           << " (" << 100.0*lost_events/(events + lost_events) << "%)";//"  last time_upper = " << (last_time & 0xffffffffc0000000);



    if (callback->override_timestamps_) {
      for (auto &q : one_spill.stats)
        q.lab_time = boost::posix_time::microsec_clock::local_time();
      // livetime and realtime are not changed accordingly
    }

    if (callback->override_pause_) {
      boost::this_thread::sleep(boost::posix_time::milliseconds(callback->pause_ms_));
    } else {

    }

//    for (auto &q : one_spill.stats) {
//      if (!starts_signalled.count(q.channel)) {
//        q.stats_type = StatsType::start;
//        starts_signalled.insert(q.channel);
//      }
//    }
    spill_queue->enqueue(new Spill(one_spill));

    timeout = (callback->run_status_.load() == 2) || (callback->terminate_premature_ && (count >= callback->max_rbuf_evts_));
    if (item == NULL)
      break;
    }


    delete evt_file;
  }

  one_spill.hits.clear();

  for (auto &q : one_spill.stats)
    q.stats_type = StatsType::stop;

  spill_queue->enqueue(new Spill(one_spill));

  callback->run_status_.store(3);

  PL_DBG << "<SorterEVT> Stop run worker";

}

std::string SorterEVT::buffer_to_string(const std::list<uint32_t>& buffer) {
  std::ostringstream out2;
  int j=0;
  for (auto &q : buffer) {
    if (j && ( (j % 4) == 0))
      out2 << std::endl;
    out2 << itobin32(q) << "   ";
    j++;
  }
  return out2.str();
}

Spill SorterEVT::get_spill() {
  Spill one_spill;

  return one_spill;
}


}
