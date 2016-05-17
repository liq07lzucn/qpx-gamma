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
 *      Fitter
 *
 ******************************************************************************/

#ifndef QPX_FITTER_H
#define QPX_FITTER_H

#include "peak.h"
#include "roi.h"
#include "daq_sink.h"
#include "finder.h"

namespace Qpx {

class Fitter {
  
public:
  Fitter() :
    activity_scale_factor_(1.0)
  {
    finder_.settings_ = settings_;
  }

  const FitSettings &settings() {
    return settings_;
  }

  void apply_settings(FitSettings settings);
//  void apply_energy_calibration(Calibration cal);
//  void apply_fwhm_calibration(Calibration cal);

  Fitter(SinkPtr spectrum)
      : Fitter()
  {setData(spectrum);}

  void clear();
  
  void setData(SinkPtr spectrum);

  void find_regions();
  ROI *parent_of(double center);

  std::set<double> relevant_regions(double left, double right);

  bool merge_regions(double left, double right, boost::atomic<bool>& interruptor);
  void add_peak(double left, double right, boost::atomic<bool>& interruptor);
  bool adj_LB(double regionL, double left, double right, boost::atomic<bool>& interruptor);
  bool adj_RB(double regionL, double left, double right, boost::atomic<bool>& interruptor);
  bool adjust_sum4(double &peak_center, double left, double right);

  void remove_peaks(std::set<double> bins);
  void delete_ROI(double bin);
  void replace_peak(const Peak&);

  void save_report(std::string filename);

  std::map<double, Peak> peaks();


  void to_xml(pugi::xml_node &node) const;
  void from_xml(const pugi::xml_node &node, SinkPtr spectrum);
  std::string xml_element_name() const {return "Fitter";}



  //for efficiency stuff
  std::string sample_name_;
  double activity_scale_factor_; //should be in spectrum?

  Detector detector_; //need this? metadata?

  
  //data from spectrum
  Finder finder_;
  Metadata metadata_;

  //actual results
  std::map<double, ROI> regions_;

private:
  FitSettings settings_;

};

}

#endif