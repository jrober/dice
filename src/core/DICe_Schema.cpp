// @HEADER
// ************************************************************************
//
//               Digital Image Correlation Engine (DICe)
//                 Copyright 2015 Sandia Corporation.
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact: Dan Turner (dzturne@sandia.gov)
//
// ************************************************************************
// @HEADER

#include <DICe.h>
#include <DICe_Schema.h>
#include <DICe_ObjectiveZNSSD.h>
#include <DICe_PostProcessor.h>
#include <DICe_ParameterUtilities.h>

#include <Teuchos_XMLParameterListHelpers.hpp>
#include <Teuchos_ArrayRCP.hpp>

#include <ctime>
#include <iostream>
#include <fstream>
#include <algorithm>

#include <cassert>
#include <set>

#ifndef   DICE_DISABLE_BOOST_FILESYSTEM
#include    <boost/filesystem.hpp>
#endif

namespace DICe {

Schema::Schema(const int_t img_width,
  const int_t img_height,
  const intensity_t initial_intensity_value,
  const Teuchos::RCP<Teuchos::ParameterList> & params){
  default_constructor_tasks(params);
  ref_img_ = Teuchos::rcp( new Image(img_width,img_height,initial_intensity_value));
  Teuchos::RCP<Image> def_img = Teuchos::rcp( new Image(img_width,img_height,initial_intensity_value));
  def_imgs_.push_back(def_img);
  Teuchos::RCP<Image> prev_img = Teuchos::rcp( new Image(img_width,img_height,initial_intensity_value));
  prev_imgs_.push_back(prev_img);
  // require that the images are the same size
  TEUCHOS_TEST_FOR_EXCEPTION(ref_img_->width()<=0||ref_img_->width()!=def_imgs_[0]->width(),std::runtime_error,
    "Error: Images must be the same width and nonzero.");
  TEUCHOS_TEST_FOR_EXCEPTION(ref_img_->height()<=0||ref_img_->height()!=def_imgs_[0]->height(),std::runtime_error,
    "Error: Images must be the same height and nonzero.");
}

Schema::Schema(const std::string & refName,
  const std::string & defName,
  const std::string & params_file_name){
  // create a parameter list from the selected file
  Teuchos::RCP<Teuchos::ParameterList> params = read_correlation_params(params_file_name);
  construct_schema(refName,defName,params);
}

Schema::Schema(const std::string & refName,
  const std::string & defName,
  const Teuchos::RCP<Teuchos::ParameterList> & params){
  construct_schema(refName,defName,params);
}

void
Schema::construct_schema(const std::string & refName,
  const std::string & defName,
  const Teuchos::RCP<Teuchos::ParameterList> & params){
  default_constructor_tasks(params);

  Teuchos::RCP<Teuchos::ParameterList> imgParams;
  if(params!=Teuchos::null) imgParams = params;
  else imgParams = Teuchos::rcp(new Teuchos::ParameterList());
  // construct the images
  // (the compute_image_gradients param is used by the image constructor)
  imgParams->set(DICe::compute_image_gradients,compute_ref_gradients_);
  imgParams->set(DICe::gauss_filter_images,gauss_filter_images_);
  imgParams->set(DICe::gauss_filter_mask_size,gauss_filter_mask_size_);
  ref_img_ = Teuchos::rcp( new Image(refName.c_str(),imgParams));
  Teuchos::RCP<Image> prev_img = Teuchos::rcp( new Image(refName.c_str(),imgParams));
  if(prev_imgs_.size()==0) prev_imgs_.push_back(prev_img);
  else prev_imgs_[0] = prev_img;
  // (the compute_image_gradients param is used by the image constructor)
  imgParams->set(DICe::compute_image_gradients,compute_def_gradients_);
  Teuchos::RCP<Image> def_img = Teuchos::rcp( new Image(defName.c_str(),imgParams));
  if(def_imgs_.size()==0) def_imgs_.push_back(def_img);
  else def_imgs_[0] = def_img;
  if(ref_image_rotation_!=ZERO_DEGREES){
    ref_img_ = ref_img_->apply_rotation(ref_image_rotation_,imgParams);
    prev_imgs_[0] = prev_imgs_[0]->apply_rotation(ref_image_rotation_,imgParams);
  }
  if(def_image_rotation_!=ZERO_DEGREES){
    def_imgs_[0] = def_imgs_[0]->apply_rotation(def_image_rotation_,imgParams);
  }
  const int_t width = ref_img_->width();
  const int_t height = ref_img_->height();
  // require that the images are the same size
  TEUCHOS_TEST_FOR_EXCEPTION(width!=def_imgs_[0]->width(),std::runtime_error,"Error, Images must be the same width.");
  TEUCHOS_TEST_FOR_EXCEPTION(height!=def_imgs_[0]->height(),std::runtime_error,"Error, Images must be the same height.");
}

Schema::Schema(const int_t img_width,
  const int_t img_height,
  const Teuchos::ArrayRCP<intensity_t> refRCP,
  const Teuchos::ArrayRCP<intensity_t> defRCP,
  const std::string & params_file_name){
  // create a parameter list from the selected file
  Teuchos::RCP<Teuchos::ParameterList> params = read_correlation_params(params_file_name);
  construct_schema(img_width,img_height,refRCP,defRCP,params);
}

Schema::Schema(const int_t img_width,
  const int_t img_height,
  const Teuchos::ArrayRCP<intensity_t> refRCP,
  const Teuchos::ArrayRCP<intensity_t> defRCP,
  const Teuchos::RCP<Teuchos::ParameterList> & params){
  construct_schema(img_width,img_height,refRCP,defRCP,params);
}

void
Schema::construct_schema(const int_t img_width,
  const int_t img_height,
  const Teuchos::ArrayRCP<intensity_t> refRCP,
  const Teuchos::ArrayRCP<intensity_t> defRCP,
  const Teuchos::RCP<Teuchos::ParameterList> & params){

  default_constructor_tasks(params);

  Teuchos::RCP<Teuchos::ParameterList> imgParams;
  if(params!=Teuchos::null) imgParams = params;
  else imgParams = Teuchos::rcp(new Teuchos::ParameterList());

  // (the compute_image_gradients param is used by the image constructor)
  imgParams->set(DICe::compute_image_gradients,compute_ref_gradients_);
  imgParams->set(DICe::gauss_filter_images,gauss_filter_images_);
  imgParams->set(DICe::gauss_filter_mask_size,gauss_filter_mask_size_);
  ref_img_ = Teuchos::rcp( new Image(img_width,img_height,refRCP,imgParams));
  Teuchos::RCP<Image> prev_img = Teuchos::rcp( new Image(img_width,img_height,refRCP,imgParams));
  if(prev_imgs_.size()==0) prev_imgs_.push_back(prev_img);
  else prev_imgs_[0] = prev_img;
  imgParams->set(DICe::compute_image_gradients,compute_def_gradients_);
  Teuchos::RCP<Image> def_img = Teuchos::rcp( new Image(img_width,img_height,defRCP,imgParams));
  if(def_imgs_.size()==0) def_imgs_.push_back(def_img);
  else def_imgs_[0] = def_img;
  if(ref_image_rotation_!=ZERO_DEGREES){
    ref_img_ = ref_img_->apply_rotation(ref_image_rotation_,imgParams);
    prev_imgs_[0] = prev_imgs_[0]->apply_rotation(ref_image_rotation_,imgParams);
  }
  if(def_image_rotation_!=ZERO_DEGREES){
    def_imgs_[0] = def_imgs_[0]->apply_rotation(def_image_rotation_,imgParams);
  }
  // require that the images are the same size
  TEUCHOS_TEST_FOR_EXCEPTION(ref_img_->width()<=0||ref_img_->width()!=def_imgs_[0]->width(),std::runtime_error,
    "Error: Images must be the same width and nonzero.");
  TEUCHOS_TEST_FOR_EXCEPTION(ref_img_->height()<=0||ref_img_->height()!=def_imgs_[0]->height(),std::runtime_error,
    "Error: Images must be the same height and nonzero.");
}

Schema::Schema(Teuchos::RCP<Image> ref_img,
  Teuchos::RCP<Image> def_img,
  const std::string & params_file_name){
  // create a parameter list from the selected file
  Teuchos::RCP<Teuchos::ParameterList> params = read_correlation_params(params_file_name);
  construct_schema(ref_img,def_img,params);
}

Schema::Schema(Teuchos::RCP<Image> ref_img,
  Teuchos::RCP<Image> def_img,
  const Teuchos::RCP<Teuchos::ParameterList> & params){
  construct_schema(ref_img,def_img,params);
}

void
Schema::construct_schema(Teuchos::RCP<Image> ref_img,
  Teuchos::RCP<Image> def_img,
  const Teuchos::RCP<Teuchos::ParameterList> & params)
{
  default_constructor_tasks(params);
  if(gauss_filter_images_){
    if(!ref_img->has_gauss_filter()) // the filter may have alread been applied to the image
      ref_img->gauss_filter(gauss_filter_mask_size_);
    if(!def_img->has_gauss_filter())
      def_img->gauss_filter(gauss_filter_mask_size_);
  }
  ref_img_ = ref_img;
  if(def_imgs_.size()==0) def_imgs_.push_back(def_img);
  else def_imgs_[0] = def_img;
  if(prev_imgs_.size()==0) prev_imgs_.push_back(ref_img);
  else prev_imgs_[0] = ref_img;
  if(ref_image_rotation_!=ZERO_DEGREES){
    ref_img_ = ref_img_->apply_rotation(ref_image_rotation_);
    prev_imgs_[0] = prev_imgs_[0]->apply_rotation(ref_image_rotation_);
  }
  if(def_image_rotation_!=ZERO_DEGREES){
    def_imgs_[0] = def_imgs_[0]->apply_rotation(def_image_rotation_);
  }
  if(compute_ref_gradients_&&!ref_img_->has_gradients()){
    ref_img_->compute_gradients();
  }
  if(compute_def_gradients_&&!def_imgs_[0]->has_gradients()){
    def_imgs_[0]->compute_gradients();
  }
}

void
Schema::rotate_def_image(){
  if(def_image_rotation_!=ZERO_DEGREES){
    for(size_t i=0;i<def_imgs_.size();++i)
      def_imgs_[i] = def_imgs_[i]->apply_rotation(def_image_rotation_);
  }
}

void
Schema::set_def_image(const std::string & defName,
  const int_t id){
  DEBUG_MSG("Schema: Resetting the deformed image");
  assert(def_imgs_.size()>0);
  assert(id<(int_t)def_imgs_.size());
  Teuchos::RCP<Teuchos::ParameterList> imgParams = Teuchos::rcp(new Teuchos::ParameterList());
  imgParams->set(DICe::compute_image_gradients,compute_def_gradients_);
  imgParams->set(DICe::gauss_filter_images,gauss_filter_images_);
  imgParams->set(DICe::gauss_filter_mask_size,gauss_filter_mask_size_);
  def_imgs_[id] = Teuchos::rcp( new Image(defName.c_str(),imgParams));
  TEUCHOS_TEST_FOR_EXCEPTION(def_imgs_[id]->width()!=ref_img_->width()||def_imgs_[id]->height()!=ref_img_->height(),
    std::runtime_error,"Error, ref and def images must have the same dimensions");
  if(def_image_rotation_!=ZERO_DEGREES){
    def_imgs_[id] = def_imgs_[id]->apply_rotation(def_image_rotation_);
  }
}

void
Schema::set_def_image(Teuchos::RCP<Image> img,
  const int_t id){
  DEBUG_MSG("Schema::set_def_image() Resetting the deformed image for sub image id " << id);
  assert(def_imgs_.size()>0);
  assert(id<(int_t)def_imgs_.size());
  def_imgs_[id] = img;
  if(def_image_rotation_!=ZERO_DEGREES){
    def_imgs_[id] = def_imgs_[id]->apply_rotation(def_image_rotation_);
  }
}

void
Schema::set_prev_image(Teuchos::RCP<Image> img,
  const int_t id){
  DEBUG_MSG("Schema::set_prev_image() Resetting the previous image for sub image id " << id);
  assert(prev_imgs_.size()>0);
  assert(id<(int_t)prev_imgs_.size());
  prev_imgs_[id] = img;
}

void
Schema::set_def_image(const int_t img_width,
  const int_t img_height,
  const Teuchos::ArrayRCP<intensity_t> defRCP,
  const int_t id){
  DEBUG_MSG("Schema:  Resetting the deformed image");
  assert(def_imgs_.size()>0);
  assert(id<(int_t)def_imgs_.size());
  TEUCHOS_TEST_FOR_EXCEPTION(img_width<=0,std::runtime_error,"");
  TEUCHOS_TEST_FOR_EXCEPTION(img_height<=0,std::runtime_error,"");
  TEUCHOS_TEST_FOR_EXCEPTION(img_width!=ref_img_->width()||img_height!=ref_img_->height(),
    std::runtime_error,"Error, ref and def images must have the same dimensions");
  def_imgs_[id] = Teuchos::rcp( new Image(img_width,img_height,defRCP));
  if(def_image_rotation_!=ZERO_DEGREES){
    def_imgs_[id] = def_imgs_[id]->apply_rotation(def_image_rotation_);
  }
}

void
Schema::set_ref_image(const std::string & refName){
  DEBUG_MSG("Schema:  Resetting the reference image");
  Teuchos::RCP<Teuchos::ParameterList> imgParams = Teuchos::rcp(new Teuchos::ParameterList());
  imgParams->set(DICe::compute_image_gradients,true); // automatically compute the gradients if the ref image is changed
  ref_img_ = Teuchos::rcp( new Image(refName.c_str(),imgParams));
  if(ref_image_rotation_!=ZERO_DEGREES){
    ref_img_ = ref_img_->apply_rotation(ref_image_rotation_,imgParams);
  }
}

void
Schema::set_ref_image(const int_t img_width,
  const int_t img_height,
  const Teuchos::ArrayRCP<intensity_t> refRCP){
  DEBUG_MSG("Schema:  Resetting the reference image");
  TEUCHOS_TEST_FOR_EXCEPTION(img_width<=0,std::runtime_error,"");
  TEUCHOS_TEST_FOR_EXCEPTION(img_height<=0,std::runtime_error,"");
  Teuchos::RCP<Teuchos::ParameterList> imgParams = Teuchos::rcp(new Teuchos::ParameterList());
  imgParams->set(DICe::compute_image_gradients,true); // automatically compute the gradients if the ref image is changed
  ref_img_ = Teuchos::rcp( new Image(img_width,img_height,refRCP,imgParams));
  if(ref_image_rotation_!=ZERO_DEGREES){
    ref_img_ = ref_img_->apply_rotation(ref_image_rotation_,imgParams);
  }
}

void
Schema::default_constructor_tasks(const Teuchos::RCP<Teuchos::ParameterList> & params){
  data_num_points_ = 0;
  subset_dim_ = -1;
  step_size_x_ = -1;
  step_size_y_ = -1;
  mesh_size_ = -1.0;
  image_frame_ = 0;
  first_frame_index_ = 1;
  num_image_frames_ = -1;
  has_output_spec_ = false;
  is_initialized_ = false;
  analysis_type_ = LOCAL_DIC;
  target_field_descriptor_ = ALL_OWNED;
  distributed_fields_being_modified_ = false;
  has_post_processor_ = false;
  normalize_gamma_with_active_pixels_ = false;
  gauss_filter_images_ = false;
  gauss_filter_mask_size_ = 7;
  init_params_ = params;
  comm_ = Teuchos::rcp(new MultiField_Comm());
  path_file_names_ = Teuchos::rcp(new std::map<int_t,std::string>());
  optical_flow_flags_ = Teuchos::rcp(new std::map<int_t,bool>());
  skip_solve_flags_ = Teuchos::rcp(new std::map<int_t,std::vector<int_t> >());
  motion_window_params_ = Teuchos::rcp(new std::map<int_t,Motion_Window_Params>());
  initial_gamma_threshold_ = -1.0;
  final_gamma_threshold_ = -1.0;
  path_distance_threshold_ = -1.0;
  set_params(params);
}

void
Schema::set_params(const std::string & params_file_name){
  // create a parameter list from the selected file
  Teuchos::RCP<Teuchos::ParameterList> params = Teuchos::rcp( new Teuchos::ParameterList() );
  Teuchos::Ptr<Teuchos::ParameterList> paramsPtr(params.get());
  Teuchos::updateParametersFromXmlFile(params_file_name,paramsPtr);
  set_params(params);
}

void
Schema::set_params(const Teuchos::RCP<Teuchos::ParameterList> & params){

  const int_t proc_rank = comm_->get_rank();

  if(params!=Teuchos::null){
    if(params->get<bool>(DICe::use_global_dic,false))
      analysis_type_=GLOBAL_DIC;
    // TODO make sure only one of these is active
  }

  // start with the default params and add any that are specified by the input params
  Teuchos::RCP<Teuchos::ParameterList> diceParams = Teuchos::rcp( new Teuchos::ParameterList("Schema_Correlation_Parameters") );

  if(analysis_type_==GLOBAL_DIC){
    TEUCHOS_TEST_FOR_EXCEPTION(true,std::invalid_argument,"Global DIC is not enabled");
  }
  else if(analysis_type_==LOCAL_DIC){
    bool use_tracking_defaults = false;
    if(params!=Teuchos::null){
      use_tracking_defaults = params->get<bool>(DICe::use_tracking_default_params,false);
    }
    // First set all of the params to their defaults in case the user does not specify them:
    if(use_tracking_defaults){
      tracking_default_params(diceParams.getRawPtr());
      if(proc_rank == 0) DEBUG_MSG("Initializing schema params with tracking default parameters");
    }
    else{
      dice_default_params(diceParams.getRawPtr());
      if(proc_rank == 0) DEBUG_MSG("Initializing schema params with full-field default parameters");
    }
    // Overwrite any params that are specified by the params argument
    if(params!=Teuchos::null){
      // check that all the parameters are valid:
      // this should catch the case that the user misspelled one of the parameters:
      bool allParamsValid = true;
      for(Teuchos::ParameterList::ConstIterator it=params->begin();it!=params->end();++it){
        bool paramValid = false;
        for(int_t j=0;j<DICe::num_valid_correlation_params;++j){
          if(it->first==valid_correlation_params[j].name_){
            diceParams->setEntry(it->first,it->second); // overwrite the default value with argument param specified values
            paramValid = true;
          }
        }
        // catch post processor entries
        for(int_t j=0;j<DICe::num_valid_post_processor_params;++j){
          if(it->first==valid_post_processor_params[j]){
            diceParams->setEntry(it->first,it->second); // overwrite the default value with argument param specified values
            paramValid = true;
          }
        }
        if(!paramValid){
          allParamsValid = false;
          if(proc_rank == 0) std::cout << "Error: Invalid parameter: " << it->first << std::endl;
        }
      }
      if(!allParamsValid){
        if(proc_rank == 0) std::cout << "NOTE: valid parameters include: " << std::endl;
        for(int_t j=0;j<DICe::num_valid_correlation_params;++j){
          if(proc_rank == 0) std::cout << valid_correlation_params[j].name_ << std::endl;
        }
        for(int_t j=0;j<DICe::num_valid_post_processor_params;++j){
          if(proc_rank == 0) std::cout << valid_post_processor_params[j] << std::endl;
        }
      }
      TEUCHOS_TEST_FOR_EXCEPTION(!allParamsValid,std::invalid_argument,"Invalid parameter");
    }
  }
  else{
    TEUCHOS_TEST_FOR_EXCEPTION(true,std::runtime_error,"Error, unrecognized analysis_type");
  }
#ifdef DICE_DEBUG_MSG
  if(proc_rank == 0) {
    std::cout << "Full set of correlation parameters: " << std::endl;
    diceParams->print(std::cout);
  }
#endif

  gauss_filter_images_ = diceParams->get<bool>(DICe::gauss_filter_images,false);
  gauss_filter_mask_size_ = diceParams->get<int_t>(DICe::gauss_filter_mask_size,7);
  compute_ref_gradients_ = diceParams->get<bool>(DICe::compute_ref_gradients,true);
  compute_def_gradients_ = diceParams->get<bool>(DICe::compute_def_gradients,false);
  if(diceParams->get<bool>(DICe::compute_image_gradients,false)) { // this flag turns them both on
    compute_ref_gradients_ = true;
    compute_def_gradients_ = true;
  }
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::projection_method),std::runtime_error,"");
  projection_method_ = diceParams->get<Projection_Method>(DICe::projection_method);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::interpolation_method),std::runtime_error,"");
  interpolation_method_ = diceParams->get<Interpolation_Method>(DICe::interpolation_method);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::max_evolution_iterations),std::runtime_error,"");
  max_evolution_iterations_ = diceParams->get<int_t>(DICe::max_evolution_iterations);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::max_solver_iterations_fast),std::runtime_error,"");
  max_solver_iterations_fast_ = diceParams->get<int_t>(DICe::max_solver_iterations_fast);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::fast_solver_tolerance),std::runtime_error,"");
  fast_solver_tolerance_ = diceParams->get<double>(DICe::fast_solver_tolerance);
  // make sure image gradients are on at least for the reference image for any gradient based optimization routine
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::optimization_method),std::runtime_error,"");
  optimization_method_ = diceParams->get<Optimization_Method>(DICe::optimization_method);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::correlation_routine),std::runtime_error,"");
  correlation_routine_ = diceParams->get<Correlation_Routine>(DICe::correlation_routine);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::initialization_method),std::runtime_error,"");
  initialization_method_ = diceParams->get<Initialization_Method>(DICe::initialization_method);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::max_solver_iterations_robust),std::runtime_error,"");
  max_solver_iterations_robust_ = diceParams->get<int_t>(DICe::max_solver_iterations_robust);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::robust_solver_tolerance),std::runtime_error,"");
  robust_solver_tolerance_ = diceParams->get<double>(DICe::robust_solver_tolerance);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::skip_solve_gamma_threshold),std::runtime_error,"");
  skip_solve_gamma_threshold_ = diceParams->get<double>(DICe::skip_solve_gamma_threshold);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::skip_all_solves),std::runtime_error,"");
  skip_all_solves_ = diceParams->get<bool>(DICe::skip_all_solves);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::initial_gamma_threshold),std::runtime_error,"");
  initial_gamma_threshold_ = diceParams->get<double>(DICe::initial_gamma_threshold);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::final_gamma_threshold),std::runtime_error,"");
  final_gamma_threshold_ = diceParams->get<double>(DICe::final_gamma_threshold);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::path_distance_threshold),std::runtime_error,"");
  path_distance_threshold_ = diceParams->get<double>(DICe::path_distance_threshold);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::disp_jump_tol),std::runtime_error,"");
  disp_jump_tol_ = diceParams->get<double>(DICe::disp_jump_tol);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::theta_jump_tol),std::runtime_error,"");
  theta_jump_tol_ = diceParams->get<double>(DICe::theta_jump_tol);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::robust_delta_disp),std::runtime_error,"");
  robust_delta_disp_ = diceParams->get<double>(DICe::robust_delta_disp);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::robust_delta_theta),std::runtime_error,"");
  robust_delta_theta_ = diceParams->get<double>(DICe::robust_delta_theta);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::enable_translation),std::runtime_error,"");
  enable_translation_ = diceParams->get<bool>(DICe::enable_translation);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::enable_rotation),std::runtime_error,"");
  enable_rotation_ = diceParams->get<bool>(DICe::enable_rotation);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::enable_normal_strain),std::runtime_error,"");
  enable_normal_strain_ = diceParams->get<bool>(DICe::enable_normal_strain);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::enable_shear_strain),std::runtime_error,"");
  enable_shear_strain_ = diceParams->get<bool>(DICe::enable_shear_strain);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::output_deformed_subset_images),std::runtime_error,"");
  output_deformed_subset_images_ = diceParams->get<bool>(DICe::output_deformed_subset_images);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::output_deformed_subset_intensity_images),std::runtime_error,"");
  output_deformed_subset_intensity_images_ = diceParams->get<bool>(DICe::output_deformed_subset_intensity_images);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::output_evolved_subset_images),std::runtime_error,"");
  output_evolved_subset_images_ = diceParams->get<bool>(DICe::output_evolved_subset_images);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::use_subset_evolution),std::runtime_error,"");
  use_subset_evolution_ = diceParams->get<bool>(DICe::use_subset_evolution);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::pixel_integration_order),std::runtime_error,"");
  pixel_integration_order_ = diceParams->get<int_t>(DICe::pixel_integration_order);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::obstruction_skin_factor),std::runtime_error,"");
  obstruction_skin_factor_ = diceParams->get<double>(DICe::obstruction_skin_factor);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::use_objective_regularization),std::runtime_error,"");
  use_objective_regularization_ = diceParams->get<bool>(DICe::use_objective_regularization);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::objective_regularization_factor),std::runtime_error,"");
  objective_regularization_factor_ = diceParams->get<double>(DICe::objective_regularization_factor);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::output_beta),std::runtime_error,"");
  output_beta_ = diceParams->get<bool>(DICe::output_beta);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::normalize_gamma_with_active_pixels),std::runtime_error,"");
  normalize_gamma_with_active_pixels_ = diceParams->get<bool>(DICe::normalize_gamma_with_active_pixels);
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::rotate_ref_image_90),std::runtime_error,"");
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::rotate_ref_image_180),std::runtime_error,"");
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::rotate_ref_image_270),std::runtime_error,"");
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::rotate_def_image_90),std::runtime_error,"");
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::rotate_def_image_180),std::runtime_error,"");
  TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::rotate_def_image_270),std::runtime_error,"");
  // last one read wins here:
  ref_image_rotation_ = ZERO_DEGREES;
  def_image_rotation_ = ZERO_DEGREES;
  if(diceParams->get<bool>(DICe::rotate_ref_image_90)) ref_image_rotation_ = NINTY_DEGREES;
  if(diceParams->get<bool>(DICe::rotate_ref_image_180)) ref_image_rotation_ = ONE_HUNDRED_EIGHTY_DEGREES;
  if(diceParams->get<bool>(DICe::rotate_ref_image_270)) ref_image_rotation_ = TWO_HUNDRED_SEVENTY_DEGREES;
  if(diceParams->get<bool>(DICe::rotate_def_image_90)) def_image_rotation_ = NINTY_DEGREES;
  if(diceParams->get<bool>(DICe::rotate_def_image_180)) def_image_rotation_ = ONE_HUNDRED_EIGHTY_DEGREES;
  if(diceParams->get<bool>(DICe::rotate_def_image_270)) def_image_rotation_ = TWO_HUNDRED_SEVENTY_DEGREES;
  if(normalize_gamma_with_active_pixels_)
    DEBUG_MSG("Gamma values will be normalized by the number of active pixels.");
  if(analysis_type_==GLOBAL_DIC){
    compute_ref_gradients_ = true;
    TEUCHOS_TEST_FOR_EXCEPTION(!diceParams->isParameter(DICe::use_hvm_stabilization),std::runtime_error,"");
    use_hvm_stabilization_ = diceParams->get<bool>(DICe::use_hvm_stabilization);
  }
  else{
    if(optimization_method_!=DICe::SIMPLEX) {
      compute_ref_gradients_ = true;
    }
  }
  // create all the necessary post processors
  // create any of the post processors that may have been requested
  if(diceParams->isParameter(DICe::post_process_vsg_strain)){
    Teuchos::ParameterList sublist = diceParams->sublist(DICe::post_process_vsg_strain);
    Teuchos::RCP<Teuchos::ParameterList> ppParams = Teuchos::rcp( new Teuchos::ParameterList());
    for(Teuchos::ParameterList::ConstIterator it=sublist.begin();it!=sublist.end();++it){
      ppParams->setEntry(it->first,it->second);
    }
    Teuchos::RCP<VSG_Strain_Post_Processor> vsg_ptr = Teuchos::rcp (new VSG_Strain_Post_Processor(this,ppParams));
    post_processors_.push_back(vsg_ptr);
  }
  if(diceParams->isParameter(DICe::post_process_nlvc_strain)){
    Teuchos::ParameterList sublist = diceParams->sublist(DICe::post_process_nlvc_strain);
    Teuchos::RCP<Teuchos::ParameterList> ppParams = Teuchos::rcp( new Teuchos::ParameterList());
    for(Teuchos::ParameterList::ConstIterator it=sublist.begin();it!=sublist.end();++it){
      ppParams->setEntry(it->first,it->second);
    }
    Teuchos::RCP<NLVC_Strain_Post_Processor> nlvc_ptr = Teuchos::rcp (new NLVC_Strain_Post_Processor(this,ppParams));
    post_processors_.push_back(nlvc_ptr);
  }
  if(diceParams->isParameter(DICe::post_process_keys4_strain)){
    Teuchos::ParameterList sublist = diceParams->sublist(DICe::post_process_keys4_strain);
    Teuchos::RCP<Teuchos::ParameterList> ppParams = Teuchos::rcp( new Teuchos::ParameterList());
    for(Teuchos::ParameterList::ConstIterator it=sublist.begin();it!=sublist.end();++it){
      ppParams->setEntry(it->first,it->second);
    }
    Teuchos::RCP<Keys4_Strain_Post_Processor> keys4_ptr = Teuchos::rcp (new Keys4_Strain_Post_Processor(this,ppParams));
    post_processors_.push_back(keys4_ptr);
  }
  if(diceParams->isParameter(DICe::post_process_global_strain)){
    Teuchos::ParameterList sublist = diceParams->sublist(DICe::post_process_global_strain);
    Teuchos::RCP<Teuchos::ParameterList> ppParams = Teuchos::rcp( new Teuchos::ParameterList());
    for(Teuchos::ParameterList::ConstIterator it=sublist.begin();it!=sublist.end();++it){
      ppParams->setEntry(it->first,it->second);
    }
    Teuchos::RCP<Global_Strain_Post_Processor> global_ptr = Teuchos::rcp (new Global_Strain_Post_Processor(this,ppParams));
    post_processors_.push_back(global_ptr);
  }
  if(post_processors_.size()>0) has_post_processor_ = true;

  Teuchos::RCP<Teuchos::ParameterList> outputParams;
  if(diceParams->isParameter(DICe::output_spec)){
    if(proc_rank == 0) DEBUG_MSG("Output spec was provided by user");
    // Strip output params sublist out of params
    Teuchos::ParameterList output_sublist = diceParams->sublist(DICe::output_spec);
    outputParams = Teuchos::rcp( new Teuchos::ParameterList());
    // iterate the sublist and add the params to the output params:
    for(Teuchos::ParameterList::ConstIterator it=output_sublist.begin();it!=output_sublist.end();++it){
      outputParams->setEntry(it->first,it->second);
    }
  }
  // create the output spec:
  const std::string delimiter = diceParams->get<std::string>(DICe::output_delimiter," ");
  const bool omit_row_id = diceParams->get<bool>(DICe::omit_output_row_id,false);
  output_spec_ = Teuchos::rcp(new DICe::Output_Spec(this,omit_row_id,outputParams,delimiter));
  has_output_spec_ = true;
}

void
Schema::initialize(const int_t step_size_x,
  const int_t step_size_y,
  const int_t subset_size){

  TEUCHOS_TEST_FOR_EXCEPTION(is_initialized_,std::runtime_error,"Error: this schema is already initialized.");
  TEUCHOS_TEST_FOR_EXCEPTION(subset_size<=0,std::runtime_error,"Error: width cannot be equal to or less than zero.");
  step_size_x_ = step_size_x;
  step_size_y_ = step_size_y;

  const int_t img_width = ref_img_->width();
  const int_t img_height = ref_img_->height();
  // create a buffer the size of one view along all edges
  const int_t trimmedWidth = img_width - 2*subset_size;
  const int_t trimmedHeight = img_height - 2*subset_size;
  // set up the control points
  TEUCHOS_TEST_FOR_EXCEPTION(step_size_x<=0,std::runtime_error,"Error, step size x is <= 0");
  TEUCHOS_TEST_FOR_EXCEPTION(step_size_y<=0,std::runtime_error,"Error, step size y is <= 0");
  const int_t numPointsX = trimmedWidth  / step_size_x + 1;
  const int_t numPointsY = trimmedHeight / step_size_y + 1;
  TEUCHOS_TEST_FOR_EXCEPTION(numPointsX<=0,std::runtime_error,"Error, numPointsX <= 0.");
  TEUCHOS_TEST_FOR_EXCEPTION(numPointsY<=0,std::runtime_error,"Error, numPointsY <= 0.");

  const int_t num_pts = numPointsX * numPointsY;

  initialize(num_pts,subset_size);
  assert(data_num_points_==num_pts);

  int_t x_it=0, y_it=0, x_coord=0, y_coord=0;
  for (int_t i=0;i<num_pts;++i)
  {
     y_it = i / numPointsX;
     x_it = i - (y_it*numPointsX);
     x_coord = subset_dim_ + x_it * step_size_x_ -1;
     y_coord = subset_dim_ + y_it * step_size_y_ -1;
     field_value(i,COORDINATE_X) = x_coord;
     field_value(i,COORDINATE_Y) = y_coord;
  }
}

void
Schema::initialize(const std::string & params_file_name){
  // create a parameter list from the selected file
  Teuchos::RCP<Teuchos::ParameterList> params = Teuchos::rcp( new Teuchos::ParameterList() );
  Teuchos::Ptr<Teuchos::ParameterList> paramsPtr(params.get());
  Teuchos::updateParametersFromXmlFile(params_file_name,paramsPtr);
  initialize(params);
}

void
Schema::initialize(const Teuchos::RCP<Teuchos::ParameterList> & input_params){
  const int_t dim = 2;
  const int_t proc_rank = comm_->get_rank();
  // if the subset locations are specified in an input file, read them in (else they will be defined later)
  Teuchos::RCP<std::vector<int_t> > subset_centroids;
  Teuchos::RCP<std::vector<int_t> > neighbor_ids;
  Teuchos::RCP<DICe::Subset_File_Info> subset_info;
  int_t step_size = -1;
  int_t subset_size = -1;
  int_t num_subsets = -1;
  Teuchos::RCP<std::map<int_t,DICe::Conformal_Area_Def> > conformal_area_defs;
  Teuchos::RCP<std::map<int_t,std::vector<int_t> > > blocking_subset_ids;
  Teuchos::RCP<std::set<int_t> > force_simplex;
  const bool has_subset_file = input_params->isParameter(DICe::subset_file);
  DICe::Subset_File_Info_Type subset_info_type = DICe::SUBSET_INFO;
  if(has_subset_file){
    std::string fileName = input_params->get<std::string>(DICe::subset_file);
    subset_info = DICe::read_subset_file(fileName,img_width(),img_height());
    subset_info_type = subset_info->type;
  }
  if(!has_subset_file || subset_info_type==DICe::REGION_OF_INTEREST_INFO){
    TEUCHOS_TEST_FOR_EXCEPTION(!input_params->isParameter(DICe::step_size),std::runtime_error,
      "Error, step size has not been specified");
    step_size = input_params->get<int_t>(DICe::step_size);
    DEBUG_MSG("Correlation point centroids were not specified by the user. \nThey will be evenly distrubed in the region"
        " of interest with separation (step_size) of " << step_size << " pixels.");
    subset_centroids = Teuchos::rcp(new std::vector<int_t>());
    neighbor_ids = Teuchos::rcp(new std::vector<int_t>());
    DICe::create_regular_grid_of_correlation_points(*subset_centroids,*neighbor_ids,input_params,img_width(),img_height(),subset_info);
    num_subsets = subset_centroids->size()/dim; // divide by three because the stride is x y neighbor_id
    assert(neighbor_ids->size()==subset_centroids->size()/2);
    TEUCHOS_TEST_FOR_EXCEPTION(!input_params->isParameter(DICe::subset_size),std::runtime_error,
      "Error, the subset size has not been specified"); // required for all square subsets case
    subset_size = input_params->get<int_t>(DICe::subset_size);
  }
  else{
    TEUCHOS_TEST_FOR_EXCEPTION(subset_info==Teuchos::null,std::runtime_error,"");
    subset_centroids = subset_info->coordinates_vector;
    neighbor_ids = subset_info->neighbor_vector;
    conformal_area_defs = subset_info->conformal_area_defs;
    blocking_subset_ids = subset_info->id_sets_map;
    force_simplex = subset_info->force_simplex;
    num_subsets = subset_info->coordinates_vector->size()/dim;
    if((int_t)subset_info->conformal_area_defs->size()<num_subsets){
      // Only require this if not all subsets are conformal:
      TEUCHOS_TEST_FOR_EXCEPTION(!input_params->isParameter(DICe::subset_size),std::runtime_error,
        "Error, the subset size has not been specified");
      subset_size = input_params->get<int_t>(DICe::subset_size);
    }
  }
  TEUCHOS_TEST_FOR_EXCEPTION(subset_centroids->size()<=0,std::runtime_error,"");
  TEUCHOS_TEST_FOR_EXCEPTION(num_subsets<=0,std::runtime_error,"");

  set_step_size(step_size); // this is done just so the step_size appears in the output file header (it's not actually used)
  // let the schema know how many images there are in the sequence:

  // set the blocking subset ids if they exist
  set_obstructing_subset_ids(blocking_subset_ids);
  // set the subsets that should force the simplex method
  set_force_simplex(force_simplex);
  // initialize the schema
  initialize(num_subsets,subset_size,conformal_area_defs,neighbor_ids);

  // set the coordinates for the subsets:
  // all other values are initiliazed to zero
  for(int_t i=0;i<num_subsets;++i){
    field_value(i,DICe::COORDINATE_X) = (*subset_centroids)[i*dim + 0];
    field_value(i,DICe::COORDINATE_Y) = (*subset_centroids)[i*dim + 1];
  }
  // set the seed value if they exist
  if(subset_info!=Teuchos::null){
    if(subset_info->path_file_names->size()>0){
      set_path_file_names(subset_info->path_file_names);
    }
    if(subset_info->skip_solve_flags->size()>0){
      set_skip_solve_flags(subset_info->skip_solve_flags);
#ifdef DICE_DEBUG_MSG
      std::cout << "[DICe_DEBUG]: Schema::initialize(): skip solve flags" << std::endl;
      std::string on = "ON";
      std::string off = "OFF";
      std::string state = "";
      std::map<int_t,std::vector<int_t> >::const_iterator it=skip_solve_flags_->begin();
      for(;it!=skip_solve_flags_->end();++it){
        bool skip_on = false;
        std::cout << "[DICe_DEBUG]: Schema::initialize(): subset " << it->first << " has the following flags" << std::endl;
        for(size_t id=0;id<it->second.size();++id){
          skip_on = !skip_on;
          state = skip_on ? on : off;
          std::cout << "[DICe_DEBUG]: Schema::initialize(): at frame " << it->second[id] << " skip solve is " << state << std::endl;
        }
      }
#endif
    }
    if(subset_info->optical_flow_flags->size()>0){
      set_optical_flow_flags(subset_info->optical_flow_flags);
    }
    if(subset_info->motion_window_params->size()>0){
      // make sure not running in parallel (motion window use_subset_id may be off processor) TODO fix this, ex

      set_motion_window_params(subset_info->motion_window_params);
      // change the def image storage to be a vector of motion windows rather than one large image
      def_imgs_.resize(subset_info->num_motion_windows);
      prev_imgs_.resize(subset_info->num_motion_windows);
      for(int_t i=0;i<subset_info->num_motion_windows;++i){
        def_imgs_[i] = Teuchos::null;
        prev_imgs_[i] = Teuchos::null;
      }
    }
    if(subset_info->seed_subset_ids->size()>0){
      //has_seed(true);
      TEUCHOS_TEST_FOR_EXCEPTION(subset_info->displacement_map->size()<=0,std::runtime_error,"");
      std::map<int_t,int_t>::iterator it=subset_info->seed_subset_ids->begin();
      for(;it!=subset_info->seed_subset_ids->end();++it){
        const int_t subset_id = it->first;
        const int_t roi_id = it->second;
        TEUCHOS_TEST_FOR_EXCEPTION(subset_info->displacement_map->find(roi_id)==subset_info->displacement_map->end(),
          std::runtime_error,"");
        field_value(subset_id,DICe::DISPLACEMENT_X) = subset_info->displacement_map->find(roi_id)->second.first;
        field_value(subset_id,DICe::DISPLACEMENT_Y) = subset_info->displacement_map->find(roi_id)->second.second;
        if(proc_rank==0) DEBUG_MSG("Seeding the displacement solution for subset " << subset_id << " with ux: " <<
          field_value(subset_id,DICe::DISPLACEMENT_X) << " uy: " << field_value(subset_id,DICe::DISPLACEMENT_Y));
        if(subset_info->normal_strain_map->find(roi_id)!=subset_info->normal_strain_map->end()){
          field_value(subset_id,DICe::NORMAL_STRAIN_X) = subset_info->normal_strain_map->find(roi_id)->second.first;
          field_value(subset_id,DICe::NORMAL_STRAIN_Y) = subset_info->normal_strain_map->find(roi_id)->second.second;
          if(proc_rank==0) DEBUG_MSG("Seeding the normal strain solution for subset " << subset_id << " with ex: " <<
            field_value(subset_id,DICe::NORMAL_STRAIN_X) << " ey: " << field_value(subset_id,DICe::NORMAL_STRAIN_Y));
        }
        if(subset_info->shear_strain_map->find(roi_id)!=subset_info->shear_strain_map->end()){
          field_value(subset_id,DICe::SHEAR_STRAIN_XY) = subset_info->shear_strain_map->find(roi_id)->second;
          if(proc_rank==0) DEBUG_MSG("Seeding the shear strain solution for subset " << subset_id << " with gamma_xy: " <<
            field_value(subset_id,DICe::SHEAR_STRAIN_XY));
        }
        if(subset_info->rotation_map->find(roi_id)!=subset_info->rotation_map->end()){
          field_value(subset_id,DICe::ROTATION_Z) = subset_info->rotation_map->find(roi_id)->second;
          if(proc_rank==0) DEBUG_MSG("Seeding the rotation solution for subset " << subset_id << " with theta_z: " <<
            field_value(subset_id,DICe::ROTATION_Z));
        }
      }
    }
  }

}

void
Schema::initialize(const int_t num_pts,
  const int_t subset_size,
  Teuchos::RCP<std::map<int_t,Conformal_Area_Def> > conformal_subset_defs,
  Teuchos::RCP<std::vector<int_t> > neighbor_ids){
  assert(def_imgs_.size()>0);
  TEUCHOS_TEST_FOR_EXCEPTION(def_imgs_[0]->width()!=ref_img_->width(),std::runtime_error,"");
  TEUCHOS_TEST_FOR_EXCEPTION(def_imgs_[0]->height()!=ref_img_->height(),std::runtime_error,"");
  if(is_initialized_){
    assert(data_num_points_>0);
    assert(fields_->get_num_fields()==MAX_FIELD_NAME);
    assert(fields_nm1_->get_num_fields()==MAX_FIELD_NAME);
    return;  // no need to initialize if already done
  }
  // TODO find some way to address this (for constrained optimization, the schema doesn't need any fields)
  //assert(num_pts>0);
  data_num_points_ = num_pts;
  subset_dim_ = subset_size;

  // evenly distributed one-to-one map
  dist_map_ = Teuchos::rcp(new MultiField_Map(data_num_points_,0,*comm_));

  // all owned map (not one-to-one)
  Teuchos::Array<int_t> all_subsets(data_num_points_);
  for(int_t i=0;i<data_num_points_;++i)
    all_subsets[i] = i;
  all_map_ = Teuchos::rcp(new MultiField_Map(-1,all_subsets.view(0,all_subsets.size()),0,*comm_));

  // if there are blocking subsets, they need to be on the same processor and put in order:
  create_obstruction_dist_map();

  create_seed_dist_map(neighbor_ids);

  importer_ = Teuchos::rcp(new MultiField_Importer(*dist_map_,*all_map_));
  exporter_ = Teuchos::rcp(new MultiField_Exporter(*all_map_,*dist_map_));
  seed_importer_ = Teuchos::rcp(new MultiField_Importer(*seed_dist_map_,*all_map_));
  seed_exporter_ = Teuchos::rcp(new MultiField_Exporter(*all_map_,*seed_dist_map_));
  fields_ = Teuchos::rcp(new MultiField(all_map_,MAX_FIELD_NAME,true));
  fields_nm1_ = Teuchos::rcp(new MultiField(all_map_,MAX_FIELD_NAME,true));
#if DICE_MPI
  dist_fields_ = Teuchos::rcp(new MultiField(dist_map_,MAX_FIELD_NAME,true));
  dist_fields_nm1_ = Teuchos::rcp(new MultiField(dist_map_,MAX_FIELD_NAME,true));
  seed_dist_fields_ = Teuchos::rcp(new MultiField(seed_dist_map_,MAX_FIELD_NAME,true));
  seed_dist_fields_nm1_ = Teuchos::rcp(new MultiField(seed_dist_map_,MAX_FIELD_NAME,true));
#endif
  // initialize the conformal subset map to avoid havng to check if its null always
  if(conformal_subset_defs==Teuchos::null)
    conformal_subset_defs_ = Teuchos::rcp(new std::map<int_t,DICe::Conformal_Area_Def>);
  else
    conformal_subset_defs_ = conformal_subset_defs;

  TEUCHOS_TEST_FOR_EXCEPTION(data_num_points_<(int_t)conformal_subset_defs_->size(),std::runtime_error,
    "Error, data is not the right size, conformal_subset_defs_.size() is too large for the data array");
  // ensure that the ids in conformal subset defs are valid:
  std::map<int_t,Conformal_Area_Def>::iterator it = conformal_subset_defs_->begin();
  for( ;it!=conformal_subset_defs_->end();++it){
    assert(it->first >= 0);
    assert(it->first < data_num_points_);
  }
  // ensure that a subset size was specified if not all subsets are conformal:
  if(analysis_type_==LOCAL_DIC&&(int_t)conformal_subset_defs_->size()<data_num_points_){
    TEUCHOS_TEST_FOR_EXCEPTION(subset_size<=0,std::runtime_error,"");
  }

  // initialize the post processors
  for(size_t i=0;i<post_processors_.size();++i)
    post_processors_[i]->initialize();

  is_initialized_ = true;

  if(neighbor_ids!=Teuchos::null)
    for(int_t i=0;i<data_num_points_;++i){
      field_value(i,DICe::NEIGHBOR_ID)  = (*neighbor_ids)[i];
    }
}

void
Schema::create_obstruction_dist_map(){
  if(obstructing_subset_ids_==Teuchos::null) return;
  if(obstructing_subset_ids_->size()==0) return;

  const int_t proc_id = comm_->get_rank();
  const int_t num_procs = comm_->get_size();

  if(proc_id == 0) DEBUG_MSG("Subsets have obstruction dependencies.");
  // set up the groupings of subset ids that have to stay together:
  // Note: this assumes that the obstructions are only one relation deep
  // i.e. the blocking subset cannot itself have a subset that blocks it
  // TODO address this to make it more general
  std::set<int_t> eligible_ids;
  for(int_t i=0;i<data_num_points_;++i)
    eligible_ids.insert(i);
  std::vector<std::set<int_t> > obstruction_groups;
  std::map<int_t,int_t> earliest_id_can_appear;
  std::set<int_t> assigned_to_a_group;
  std::map<int_t,std::vector<int_t> >::iterator map_it = obstructing_subset_ids_->begin();
  for(;map_it!=obstructing_subset_ids_->end();++map_it){
    int_t greatest_subset_id_among_obst = 0;
    for(size_t j=0;j<map_it->second.size();++j)
      if(map_it->second[j] > greatest_subset_id_among_obst) greatest_subset_id_among_obst = map_it->second[j];
    earliest_id_can_appear.insert(std::pair<int_t,int_t>(map_it->first,greatest_subset_id_among_obst));

    if(assigned_to_a_group.find(map_it->first)!=assigned_to_a_group.end()) continue;
    std::set<int_t> dependencies;
    dependencies.insert(map_it->first);
    eligible_ids.erase(map_it->first);
    // gather for all the dependencies for this subset
    for(size_t j=0;j<map_it->second.size();++j){
      dependencies.insert(map_it->second[j]);
      eligible_ids.erase(map_it->second[j]);
    }
    // no search all the other obstruction sets for any ids currently in the dependency list
    std::set<int_t>::iterator dep_it = dependencies.begin();
    for(;dep_it!=dependencies.end();++dep_it){
      std::map<int_t,std::vector<int_t> >::iterator search_it = obstructing_subset_ids_->begin();
      for(;search_it!=obstructing_subset_ids_->end();++search_it){
        if(assigned_to_a_group.find(search_it->first)!=assigned_to_a_group.end()) continue;
        // if any of the ids are in the current dependency list, add the whole set:
        bool match_found = false;
        if(*dep_it==search_it->first) match_found = true;
        for(size_t k=0;k<search_it->second.size();++k){
          if(*dep_it==search_it->second[k]) match_found = true;
        }
        if(match_found){
          dependencies.insert(search_it->first);
          eligible_ids.erase(search_it->first);
          for(size_t k=0;k<search_it->second.size();++k){
            dependencies.insert(search_it->second[k]);
            eligible_ids.erase(search_it->second[k]);
          }
          // reset the dependency index because more items were added to the list
          dep_it = dependencies.begin();
          // remove this set of obstruction ids since they have already been added to a group
          assigned_to_a_group.insert(search_it->first);
        } // match found
      } // obstruction set
    } // dependency it
    obstruction_groups.push_back(dependencies);
  } // outer obstruction set it
  if(proc_id == 0) DEBUG_MSG("[PROC " << proc_id << "] There are " << obstruction_groups.size() << " obstruction groupings: ");
  std::stringstream ss;
  for(size_t i=0;i<obstruction_groups.size();++i){
    ss << "[PROC " << proc_id << "] Group: " << i << std::endl;
    std::set<int_t>::iterator j = obstruction_groups[i].begin();
    for(;j!=obstruction_groups[i].end();++j){
      ss << "[PROC " << proc_id << "]   id: " << *j << std::endl;
    }
  }
  ss << "[PROC " << proc_id << "] Eligible ids: " << std::endl;
  for(std::set<int_t>::iterator elig_it=eligible_ids.begin();elig_it!=eligible_ids.end();++elig_it){
    ss << "[PROC " << proc_id << "]   " << *elig_it << std::endl;
  }
  if(proc_id == 0) DEBUG_MSG(ss.str());

  // divy up the obstruction groups among the processors:
  int_t obst_group_gid = 0;
  std::vector<std::set<int_t> > local_subset_ids(num_procs);
  while(obst_group_gid < (int_t)obstruction_groups.size()){
    for(int_t p_id=0;p_id<num_procs;++p_id){
      if(obst_group_gid < (int_t)obstruction_groups.size()){
        //if(p_id==proc_id){
        local_subset_ids[p_id].insert(obstruction_groups[obst_group_gid].begin(),obstruction_groups[obst_group_gid].end());
        //}
        obst_group_gid++;
      }
      else break;
    }
  }
  // assign the rest based on who has the least amount of subsets
  for(std::set<int_t>::iterator elig_it = eligible_ids.begin();elig_it!=eligible_ids.end();++elig_it){
    int_t proc_with_fewest_subsets = 0;
    int_t lowest_num_subsets = data_num_points_;
    for(int_t i=0;i<num_procs;++i){
      if((int_t)local_subset_ids[i].size() <= lowest_num_subsets){
        lowest_num_subsets = local_subset_ids[i].size();
        proc_with_fewest_subsets = i;
      }
    }
    local_subset_ids[proc_with_fewest_subsets].insert(*elig_it);
  }
  // order the subset ids so that they respect the dependencies:
  std::vector<int_t> local_ids;
  std::set<int_t>::iterator set_it = local_subset_ids[proc_id].begin();
  for(;set_it!=local_subset_ids[proc_id].end();++set_it){
    // not in the list of subsets with blockers
    if(obstructing_subset_ids_->find(*set_it)==obstructing_subset_ids_->end()){
      local_ids.push_back(*set_it);
    }
    // in the list of subsets with blockers, but has no blocking ids
    else if(obstructing_subset_ids_->find(*set_it)->second.size()==0){
      local_ids.push_back(*set_it);
    }

  }
  set_it = local_subset_ids[proc_id].begin();
  for(;set_it!=local_subset_ids[proc_id].end();++set_it){
    if(obstructing_subset_ids_->find(*set_it)!=obstructing_subset_ids_->end()){
      if(obstructing_subset_ids_->find(*set_it)->second.size()>0){
        TEUCHOS_TEST_FOR_EXCEPTION(earliest_id_can_appear.find(*set_it)==earliest_id_can_appear.end(),
          std::runtime_error,"");
        local_ids.push_back(*set_it);
      }
    }
  }

  ss.str(std::string());
  ss.clear();
  ss << "[PROC " << proc_id << "] Has the following subset ids: " << std::endl;
  for(size_t i=0;i<local_ids.size();++i){
    ss << "[PROC " << proc_id << "] " << local_ids[i] <<  std::endl;
  }
  DEBUG_MSG(ss.str());

  Teuchos::ArrayView<const int_t> lids_grouped_by_obstruction(&local_ids[0],local_ids.size());
  dist_map_ = Teuchos::rcp(new MultiField_Map(data_num_points_,lids_grouped_by_obstruction,0,*comm_));
  //dist_map_->describe();
  TEUCHOS_TEST_FOR_EXCEPTION(!dist_map_->is_one_to_one(),std::runtime_error,"");

  // if this is a serial run, the ordering must be changed too
  if(num_procs==1)
    all_map_ = Teuchos::rcp(new MultiField_Map(data_num_points_,lids_grouped_by_obstruction,0,*comm_));
  //all_map_->describe();
}

void
Schema::create_seed_dist_map(Teuchos::RCP<std::vector<int_t> > neighbor_ids){
  // distribution according to seeds map (one-to-one, not all procs have entries)
  // If the initialization method is USE_NEIGHBOR_VALUES or USE_NEIGHBOR_VALUES_FIRST_STEP, the
  // first step has to have a special map that keeps all subsets that use a particular seed
  // on the same processor (the parallelism is limited to the number of seeds).
  const int_t proc_id = comm_->get_rank();
  const int_t num_procs = comm_->get_size();

  if(neighbor_ids!=Teuchos::null){
    // catch the case that this is an TRACKING_ROUTINE run, but seed values were specified for
    // the individual subsets. In that case, the seed map is not necessary because there are
    // no initializiation dependencies among subsets, but the seed map will still be used since it
    // will be activated when seeds are specified for a subset.
    if(obstructing_subset_ids_!=Teuchos::null){
      if(obstructing_subset_ids_->size()>0){
        bool print_warning = false;
        for(size_t i=0;i<neighbor_ids->size();++i){
          if((*neighbor_ids)[i]!=-1) print_warning = true;
        }
        if(print_warning && proc_id==0){
          std::cout << "*** Waring: Seed values were specified for an anlysis with obstructing subsets. " << std::endl;
          std::cout << "            These values will be used to initialize subsets for which a seed has been specified, but the seed map " << std::endl;
          std::cout << "            will be set to the distributed map because grouping subsets by obstruction trumps seed ordering." << std::endl;
          std::cout << "            Seed dependencies between neighbors will not be enforced." << std::endl;
        }
        seed_dist_map_ = dist_map_;
        return;
      }
    }
    std::vector<int_t> local_subset_gids_grouped_by_roi;
    TEUCHOS_TEST_FOR_EXCEPTION((int_t)neighbor_ids->size()!=data_num_points_,std::runtime_error,"");
    std::vector<int_t> this_group_gids;
    std::vector<std::vector<int_t> > seed_groupings;
    std::vector<std::vector<int_t> > local_seed_groupings;
    for(int_t i=data_num_points_-1;i>=0;--i){
      this_group_gids.push_back(i);
      // if this subset is a seed, break this grouping and insert it in the set
      if((*neighbor_ids)[i]==-1){
        seed_groupings.push_back(this_group_gids);
        this_group_gids.clear();
      }
    }
    // TODO order the sets by their sizes and load balance better:
    // divy up the seed_groupings round-robin style:
    int_t group_gid = 0;
    int_t local_total_id_list_size = 0;
    while(group_gid < (int_t)seed_groupings.size()){
      // reverse the order so the subsets are computed from the seed out
      for(int_t p_id=0;p_id<num_procs;++p_id){
        if(group_gid < (int_t)seed_groupings.size()){
          if(p_id==proc_id){
            std::reverse(seed_groupings[group_gid].begin(), seed_groupings[group_gid].end());
            local_seed_groupings.push_back(seed_groupings[group_gid]);
            local_total_id_list_size += seed_groupings[group_gid].size();
          }
          group_gid++;
        }
        else break;
      }
    }
    DEBUG_MSG("[PROC " << proc_id << "] Has " << local_seed_groupings.size() << " local seed grouping(s)");
    for(size_t i=0;i<local_seed_groupings.size();++i){
      DEBUG_MSG("[PROC " << proc_id << "] local group id: " << i);
      for(size_t j=0;j<local_seed_groupings[i].size();++j){
        DEBUG_MSG("[PROC " << proc_id << "] gid: " << local_seed_groupings[i][j] );
      }
    }
    // concat local subset ids:
    local_subset_gids_grouped_by_roi.reserve(local_total_id_list_size);
    for(size_t i=0;i<local_seed_groupings.size();++i){
      local_subset_gids_grouped_by_roi.insert( local_subset_gids_grouped_by_roi.end(),
        local_seed_groupings[i].begin(),
        local_seed_groupings[i].end());
    }
    Teuchos::ArrayView<const int_t> lids_grouped_by_roi(&local_subset_gids_grouped_by_roi[0],local_total_id_list_size);
    seed_dist_map_ = Teuchos::rcp(new MultiField_Map(data_num_points_,lids_grouped_by_roi,0,*comm_));
  } // end has_neighbor_ids
  else{
    seed_dist_map_ = dist_map_;
  }
}

void
Schema::execute_correlation(){

  // make sure the data is ready to go since it may have been initialized externally by an api
  assert(is_initialized_);
  assert(fields_->get_num_fields()==MAX_FIELD_NAME);
  assert(fields_nm1_->get_num_fields()==MAX_FIELD_NAME);
  assert(data_num_points_>0);

  const int_t proc_id = comm_->get_rank();
  const int_t num_procs = comm_->get_size();

  DEBUG_MSG("********************");
  std::stringstream progress;
  progress << "[PROC " << proc_id << " of " << num_procs << "] IMAGE " << image_frame_ + 1;
  if(num_image_frames_>0)
    progress << " of " << num_image_frames_;
  DEBUG_MSG(progress.str());
  DEBUG_MSG("********************");

  int_t num_local_subsets = this_proc_subset_global_ids_.size();

  // reset the motion detectors for each subset if used
  for(std::map<int_t,Teuchos::RCP<Motion_Test_Utility> >::iterator it = motion_detectors_.begin();
      it != motion_detectors_.end();++it){
    DEBUG_MSG("Resetting motion detector: " << it->first);
    it->second->reset();
  }

  // PARALLEL CASE:
  if(num_procs >1){
    // first pass for a USE_FILED_VALUES run sets up the local subset list
    // for all subsequent frames, the list remains unchanged. For this case, it
    // doesn't matter if seeding is used, because neighbor values are not needed
    if(initialization_method_==USE_FIELD_VALUES){
      target_field_descriptor_ = DISTRIBUTED;
      if(this_proc_subset_global_ids_.size()==0){
        num_local_subsets = dist_map_->get_num_local_elements();
        this_proc_subset_global_ids_ = dist_map_->get_local_element_list();
      }
    }
    // if seeding is used and the init method is USE_NEIGHBOR_VALUES_FIRST_STEP_ONLY, the first
    // frame has to be serial, the rest can be parallel
    // TODO if multiple ROIs are used, the ROIs can be split across processors
    else if(initialization_method_==USE_NEIGHBOR_VALUES_FIRST_STEP_ONLY){
      if(image_frame_==0){
        target_field_descriptor_ = DISTRIBUTED_GROUPED_BY_SEED;
        num_local_subsets = seed_dist_map_->get_num_local_elements();
        this_proc_subset_global_ids_ = seed_dist_map_->get_local_element_list();
      }
      else if(image_frame_==1){
        target_field_descriptor_ = DISTRIBUTED;
        num_local_subsets = dist_map_->get_num_local_elements();
        this_proc_subset_global_ids_ = dist_map_->get_local_element_list();
      }
      // otherwise nothing needs to be done since the maps will not need to change after step 1
    }
    /// For use neighbor values, the run has to be serial for each grouping that has a seed
    else if(initialization_method_==USE_NEIGHBOR_VALUES){
      if(image_frame_==0){
        target_field_descriptor_ = DISTRIBUTED_GROUPED_BY_SEED;
        num_local_subsets = seed_dist_map_->get_num_local_elements();
        this_proc_subset_global_ids_ = seed_dist_map_->get_local_element_list();
      }
    }
    else{
      TEUCHOS_TEST_FOR_EXCEPTION(true,std::runtime_error,"Error: unknown initialization_method in execute correlation");
    }
  }

  // SERIAL CASE:

  else{
    if(image_frame_==0){
      target_field_descriptor_ = ALL_OWNED;
      num_local_subsets = all_map_->get_num_local_elements();
      this_proc_subset_global_ids_ = all_map_->get_local_element_list();
    }
  }
#ifdef DICE_DEBUG_MSG
  std::stringstream message;
  message << std::endl;
  for(int_t i=0;i<num_local_subsets;++i){
    message << "[PROC " << proc_id << "] Owns subset global id: " << this_proc_subset_global_ids_[i] << std::endl;
  }
  DEBUG_MSG(message.str());
#endif
  DEBUG_MSG("[PROC " << proc_id << "] has target_field_descriptor " << target_field_descriptor_);

  // Complete the set up activities for the post processors
  if(image_frame_==0){
    for(size_t i=0;i<post_processors_.size();++i){
      post_processors_[i]->pre_execution_tasks();
    }
  }

  // sync the fields:
  sync_fields_all_to_dist();

  // The generic routine is typically used when the dataset involves numerous subsets,
  // but only a small number of images. In this case it's more efficient to re-allocate the
  // objectives at every step, since making them static would consume a lot of memory
  if(correlation_routine_==GENERIC_ROUTINE){
    // make sure that motion windows are not used
    TEUCHOS_TEST_FOR_EXCEPTION(motion_window_params_->size()!=0,std::runtime_error,
      "Error, motion windows are intended only for the TRACKING_ROUTINE");
    prepare_optimization_initializers();
    for(int_t subset_index=0;subset_index<num_local_subsets;++subset_index){
      Teuchos::RCP<Objective> obj = Teuchos::rcp(new Objective_ZNSSD(this,
        this_proc_subset_global_ids_[subset_index]));
      generic_correlation_routine(obj);
    }
  }
  // In this routine there are usually only a handful of subsets, but thousands of images.
  // In this case it is a lot more efficient to make the objectives static since there won't
  // be very many of them, and we can avoid the allocation cost at every step
  else if(correlation_routine_==TRACKING_ROUTINE){
    // construct the static objectives if they haven't already been constructed
    if(obj_vec_.empty()){
      for(int_t subset_index=0;subset_index<num_local_subsets;++subset_index){
        const int_t subset_gid = this_proc_subset_global_ids_[subset_index];
        DEBUG_MSG("[PROC " << proc_id << "] Adding objective to obj_vec_ " << subset_gid);
        obj_vec_.push_back(Teuchos::rcp(new Objective_ZNSSD(this,
          subset_gid)));
        // set the sub_image id for each subset:
        if(motion_window_params_->find(subset_gid)!=motion_window_params_->end()){
          const int_t use_subset_id = motion_window_params_->find(subset_gid)->second.use_subset_id_;
          const int_t sub_image_id = use_subset_id ==-1 ? motion_window_params_->find(subset_gid)->second.sub_image_id_:
              motion_window_params_->find(use_subset_id)->second.sub_image_id_;
          DEBUG_MSG("[PROC " << proc_id << "] setting the sub_image id for subset " << subset_gid << " to " << sub_image_id);
          obj_vec_[subset_index]->subset()->set_sub_image_id(sub_image_id);
        }
      }
    }
    TEUCHOS_TEST_FOR_EXCEPTION((int_t)obj_vec_.size()!=num_local_subsets,std::runtime_error,"");
    prepare_optimization_initializers();
    for(int_t subset_index=0;subset_index<num_local_subsets;++subset_index){
      check_for_blocking_subsets(this_proc_subset_global_ids_[subset_index]);
      generic_correlation_routine(obj_vec_[subset_index]);
    }
    if(output_deformed_subset_images_)
      write_deformed_subsets_image();
    for(size_t i=0;i<prev_imgs_.size();++i)
      prev_imgs_[i]=def_imgs_[i];
  }
  else
    TEUCHOS_TEST_FOR_EXCEPTION(true,std::invalid_argument,"ERROR: unknown correlation routine.");

  // sync the fields
  sync_fields_dist_to_all();

  if(proc_id==0){
    for(int_t subset_index=0;subset_index<data_num_points_;++subset_index){
      DEBUG_MSG("[PROC " << proc_id << "] Subset " << subset_index << " synced-up solution after execute_correlation() done, u: " <<
        field_value(subset_index,DISPLACEMENT_X) << " v: " << field_value(subset_index,DISPLACEMENT_Y)
        << " theta: " << field_value(subset_index,ROTATION_Z) << " sigma: " << field_value(subset_index,SIGMA) << " gamma: " << field_value(subset_index,GAMMA) << " beta: " << field_value(subset_index,BETA));
    }
  }

  // compute post-processed quantities
  // For now, this assumes that all the fields are synched so that everyone owns all values.
  // TODO In the future, this can be parallelized
  // Complete the set up activities for the post processors
  // TODO maybe only processor 0 does this
  for(size_t i=0;i<post_processors_.size();++i){
    post_processors_[i]->execute();
  }
  update_image_frame();
};

void
Schema::prepare_optimization_initializers(){
  // method only needs to be called once, return if the pointers are alread addressed
  if(opt_initializers_.size()>0){
    DEBUG_MSG("Repeat call to prepare_optimization_initializers(), calling pre_execution_tasks");
    for(std::map<int_t,Teuchos::RCP<Initializer> >::iterator opt_it = opt_initializers_.begin();
        opt_it != opt_initializers_.end();++opt_it){
      //assert(*opt_it!=Teuchos::null);
      opt_it->second->pre_execution_tasks();
    }
    return;
  }

  // set up the default initializer
  Teuchos::RCP<Initializer> default_initializer;
  if(initialization_method_==USE_PHASE_CORRELATION){
    DEBUG_MSG("Default initializer is phase correlation initializer");
    default_initializer = Teuchos::rcp(new Phase_Correlation_Initializer(this));
  }
  else if(initialization_method_==USE_ZEROS){
    DEBUG_MSG("Default initializer is zero value initializer");
    default_initializer = Teuchos::rcp(new Zero_Value_Initializer(this));
  }
  else if(initialization_method_==USE_OPTICAL_FLOW){
    // make syre tga the correlation routine is tracking routine
    TEUCHOS_TEST_FOR_EXCEPTION(correlation_routine_!=TRACKING_ROUTINE,std::invalid_argument,"Error, USE_OPTICAL_FLOW "
        "initialization method is only available for the TRACKING_ROUTINE correlation_routine.");
  }
  else{ // use field values
    DEBUG_MSG("Default initializer is field value initializer");
    default_initializer = Teuchos::rcp(new Field_Value_Initializer(this));
  }

  // create the optimization initializers (one for each subset for tracking)
  if(correlation_routine_==TRACKING_ROUTINE){
    // iterate all the subetsets to see if any have path files associated
    for(size_t i=0;i<obj_vec_.size();++i){
      const int_t subset_gid = obj_vec_[i]->correlation_point_global_id();
      const bool has_path_file = path_file_names_->find(subset_gid)!=path_file_names_->end();
      if(has_path_file){ // path file will trump optical flow if it exists
        const int_t num_neighbors = 6; // number of path neighbors to search while initializing
        std::string path_file_name = path_file_names_->find(subset_gid)->second;
        DEBUG_MSG("Subset " << subset_gid << " using path file " << path_file_name << " as initializer");
        opt_initializers_.insert(std::pair<int_t,Teuchos::RCP<Initializer> >(subset_gid,
          Teuchos::rcp(new Path_Initializer(this,obj_vec_[i]->subset(),path_file_name.c_str(),num_neighbors))));
      }
      // optical flow was requested for all subsets
      else if(initialization_method_==USE_OPTICAL_FLOW){
        DEBUG_MSG("Subset " << subset_gid << " using optical flow initializer (as requested by general "
            "initialization_method in correlation params file)");
        opt_initializers_.insert(std::pair<int_t,Teuchos::RCP<Initializer> >(subset_gid,
          Teuchos::rcp(new Optical_Flow_Initializer(this,obj_vec_[i]->subset()))));
      }
      // optical flow was requested for this subset specfically
      else if(optical_flow_flags_->find(subset_gid)!=optical_flow_flags_->end()){
        if(optical_flow_flags_->find(subset_gid)->second ==true){
          TEUCHOS_TEST_FOR_EXCEPTION(has_path_file,std::runtime_error,"Error, cannot set USE_PATH_FILE and USE_OPTICAL_FLOW "
              "for the same subset in the subset file");
          DEBUG_MSG("Subset " << subset_gid << " using optical flow initializer "
              "(as requested in subset file specifically for this subset)");
          opt_initializers_.insert(std::pair<int_t,Teuchos::RCP<Initializer> >(subset_gid,
            Teuchos::rcp(new Optical_Flow_Initializer(this,obj_vec_[i]->subset()))));
        }
      }
      else{
        DEBUG_MSG("Subset " << subset_gid << " using default initializer");
        opt_initializers_.insert(std::pair<int_t,Teuchos::RCP<Initializer> >(subset_gid,default_initializer));
      }
    } // end obj_vec
  }
  // if not tracking routine use one master initializer for all subsets
  else{
    // make sure that path files were not requested
    if(path_file_names_->size()>0){
      TEUCHOS_TEST_FOR_EXCEPTION(true,std::invalid_argument,"Error, path files cannot be used with the GENERIC_ROUTINE correlation routine");
    }
    opt_initializers_.insert(std::pair<int_t,Teuchos::RCP<Initializer> >(0,default_initializer));
  }

  // call pre-correlation tasks for initializers
  for(std::map<int_t,Teuchos::RCP<Initializer> >::iterator opt_it = opt_initializers_.begin();
      opt_it != opt_initializers_.end();++opt_it){
    opt_it->second->pre_execution_tasks();
  }
}

bool
Schema::motion_detected(const int_t subset_gid){
  DEBUG_MSG("Schema::motion_detected() called");
  if(motion_window_params_->find(subset_gid)!=motion_window_params_->end()){
    // if there is no motion detection turned on, always return true (that motion is detected)
    if(motion_window_params_->find(subset_gid)->second.use_motion_detection_==false) return true;
    const int_t use_subset_id = motion_window_params_->find(subset_gid)->second.use_subset_id_==-1 ? subset_gid:
        motion_window_params_->find(subset_gid)->second.use_subset_id_;
    const int_t sub_image_id = motion_window_params_->find(subset_gid)->second.sub_image_id_;
    if(motion_detectors_.find(use_subset_id)==motion_detectors_.end()){
      // create the motion detector because it doesn't exist
      DEBUG_MSG("Creating a motion test utility for subset " << subset_gid << " using id " << use_subset_id);
      Motion_Window_Params mwp = motion_window_params_->find(use_subset_id)->second;
      motion_detectors_.insert(std::pair<int_t,Teuchos::RCP<Motion_Test_Utility> >(use_subset_id,Teuchos::rcp(new Motion_Test_Utility(this,mwp.tol_))));
    }
    TEUCHOS_TEST_FOR_EXCEPTION(motion_detectors_.find(use_subset_id)==motion_detectors_.end(),std::runtime_error,
      "Error, the motion detector should exist here, but it doesn't.");
    bool motion_det = motion_detectors_.find(use_subset_id)->second->motion_detected(sub_image_id);
    DEBUG_MSG("Subset " << subset_gid << " TEST_FOR_MOTION using window defined for subset " << use_subset_id <<
      " result " << motion_det);
    return motion_det;
  }
  else{
    DEBUG_MSG("Subset " << subset_gid << " will not test for motion");
    return true;
  }
}

void
Schema::record_failed_step(const int_t subset_gid,
  const int_t status,
  const int_t num_iterations){
  DEBUG_MSG("Subset " << subset_gid << " record failed step");
  local_field_value(subset_gid,SIGMA) = -1.0;
  local_field_value(subset_gid,MATCH) = -1.0;
  local_field_value(subset_gid,GAMMA) = -1.0;
  local_field_value(subset_gid,BETA) = -1.0;
  local_field_value(subset_gid,NOISE_LEVEL) = -1.0;
  local_field_value(subset_gid,CONTRAST_LEVEL) = -1.0;
  local_field_value(subset_gid,ACTIVE_PIXELS) = -1.0;
  local_field_value(subset_gid,STATUS_FLAG) = status;
  local_field_value(subset_gid,ITERATIONS) = num_iterations;
}

void
Schema::record_step(const int_t subset_gid,
  Teuchos::RCP<std::vector<scalar_t> > & deformation,
  const scalar_t & sigma,
  const scalar_t & match,
  const scalar_t & gamma,
  const scalar_t & beta,
  const scalar_t & noise,
  const scalar_t & contrast,
  const int_t active_pixels,
  const int_t status,
  const int_t num_iterations){
  DEBUG_MSG("Subset " << subset_gid << " record step");
  local_field_value(subset_gid,DISPLACEMENT_X) = (*deformation)[DISPLACEMENT_X];
  local_field_value(subset_gid,DISPLACEMENT_Y) = (*deformation)[DISPLACEMENT_Y];
  local_field_value(subset_gid,NORMAL_STRAIN_X) = (*deformation)[NORMAL_STRAIN_X];
  local_field_value(subset_gid,NORMAL_STRAIN_Y) = (*deformation)[NORMAL_STRAIN_Y];
  local_field_value(subset_gid,SHEAR_STRAIN_XY) = (*deformation)[SHEAR_STRAIN_XY];
  local_field_value(subset_gid,ROTATION_Z) = (*deformation)[DICe::ROTATION_Z];
  local_field_value(subset_gid,SIGMA) = sigma;
  local_field_value(subset_gid,MATCH) = match; // 0 means data is successful
  local_field_value(subset_gid,GAMMA) = gamma;
  local_field_value(subset_gid,BETA) = beta;
  local_field_value(subset_gid,NOISE_LEVEL) = noise;
  local_field_value(subset_gid,CONTRAST_LEVEL) = contrast;
  local_field_value(subset_gid,ACTIVE_PIXELS) = active_pixels;
  local_field_value(subset_gid,STATUS_FLAG) = status;
  local_field_value(subset_gid,ITERATIONS) = num_iterations;
}

Status_Flag
Schema::initial_guess(const int_t subset_gid,
  Teuchos::RCP<std::vector<scalar_t> > deformation){
  // for non-tracking routines, there is only the zero-th entry in the initializers map
  int_t sid = 0;
  // tracking routine has a different initializer for each subset
  if(correlation_routine_==TRACKING_ROUTINE){
    sid = subset_gid;
  }
  TEUCHOS_TEST_FOR_EXCEPTION(opt_initializers_.find(sid)==opt_initializers_.end(),std::runtime_error,
    "Initializer does not exist, but should here");
  return opt_initializers_.find(sid)->second->initial_guess(subset_gid,deformation);
}

void
Schema::generic_correlation_routine(Teuchos::RCP<Objective> obj){

  const int_t subset_gid = obj->correlation_point_global_id();
  TEUCHOS_TEST_FOR_EXCEPTION(get_local_id(subset_gid)==-1,std::runtime_error,
    "Error: subset id is not local to this process.");
  DEBUG_MSG("[PROC " << comm_->get_rank() << "] SUBSET " << subset_gid << " (" << local_field_value(subset_gid,DICe::COORDINATE_X) <<
    "," << local_field_value(subset_gid,DICe::COORDINATE_Y) << ")");

  // check if the solve should be skipped
  bool skip_frame = false;
  if(skip_solve_flags_->find(subset_gid)!=skip_solve_flags_->end()||skip_all_solves_){
    if(skip_solve_flags_->find(subset_gid)!=skip_solve_flags_->end()){
      // determine for this subset id if it should be skipped:
      const int_t trigger_based_frame = image_frame_ + first_frame_index_;
      DEBUG_MSG("Subset " << subset_gid << " checking skip solve for trigger based frame id " << trigger_based_frame);
      skip_frame = frame_should_be_skipped(trigger_based_frame,skip_solve_flags_->find(subset_gid)->second);
      DEBUG_MSG("Subset " << subset_gid << " frame_should_be_skipped return value: " << skip_frame);
    }
  }
  //
  //  test for motion if requested by the user in the subsets.txt file
  //
  bool motion = true;
  if(!skip_frame&&!skip_all_solves_)
    motion = motion_detected(subset_gid);
  if(!motion){
    DEBUG_MSG("Subset " << subset_gid << " skipping frame due to no motion");
    // only change the match value and the status flag
    local_field_value(subset_gid,MATCH) = 0.0;
    local_field_value(subset_gid,STATUS_FLAG) = static_cast<int_t>(FRAME_SKIPPED_DUE_TO_NO_MOTION);
    local_field_value(subset_gid,ITERATIONS) = 0;
    return;
  }
  //
  //  initial guess for the subset's solution parameters
  //
  Status_Flag init_status = INITIALIZE_SUCCESSFUL;
  Status_Flag corr_status = CORRELATION_FAILED;
  int_t num_iterations = -1;
  Teuchos::RCP<std::vector<scalar_t> > deformation = Teuchos::rcp(new std::vector<scalar_t>(DICE_DEFORMATION_SIZE,0.0));
  try{
    init_status = initial_guess(subset_gid,deformation);
  }
  catch (std::logic_error &err) { // a non-graceful exception occurred in initialization
    record_failed_step(subset_gid,static_cast<int_t>(INITIALIZE_FAILED_BY_EXCEPTION),num_iterations);
    return;
  };
  //
  //  check if initialization was successful
  //
  if(init_status==INITIALIZE_FAILED){
    record_failed_step(subset_gid,static_cast<int_t>(init_status),num_iterations);
    return;
  }
  //
  //  check if the user rrequested to skip the solve and only initialize (param set in subset file)
  //
  if(skip_frame||skip_all_solves_){
    if(skip_all_solves_){
      DEBUG_MSG("Subset " << subset_gid << " skip solve (skip_all_solves parameter was set)");
    }else{
      DEBUG_MSG("Subset " << subset_gid << " skip solve (as requested in the subset file via SKIP_SOLVE keyword)");
    }
    scalar_t noise_std_dev = 0.0;
    const scalar_t initial_sigma = obj->sigma(deformation,noise_std_dev);
    const scalar_t initial_gamma = obj->gamma(deformation);
    const scalar_t initial_beta = output_beta_ ? obj->beta(deformation) : 0.0;
    const scalar_t contrast = obj->subset()->contrast_std_dev();
    const int_t active_pixels = obj->subset()->num_active_pixels();
    record_step(subset_gid,
      deformation,initial_sigma,0.0,initial_gamma,initial_beta,
      noise_std_dev,contrast,active_pixels,static_cast<int_t>(FRAME_SKIPPED),num_iterations);
    // evolve the subsets and output the images requested as well
    // turn on pixels that at the beginning were hidden behind an obstruction
    if(use_subset_evolution_&&image_frame_>1){
      DEBUG_MSG("[PROC " << comm_->get_rank() << "] Evolving subset " << subset_gid << " using newly exposed pixels for intensity values");
      obj->subset()->turn_on_previously_obstructed_pixels();
    }
    //  Write debugging images if requested
    if(output_deformed_subset_intensity_images_)
      write_deformed_subset_intensity_image(obj);
    if(output_evolved_subset_images_)
      write_reference_subset_intensity_image(obj);
    return;
  }

  //
  //  if user requested testing the initial value of gamma, do that here
  //
  if(initial_gamma_threshold_!=-1.0){
    const scalar_t initial_gamma = obj->gamma(deformation);
    if(initial_gamma > initial_gamma_threshold_){
      DEBUG_MSG("Subset " << subset_gid << " initial gamma value FAILS threshold test, gamma: " <<
        initial_gamma << " (threshold: " << initial_gamma_threshold_ << ")");
      record_failed_step(subset_gid,static_cast<int_t>(INITIALIZE_FAILED),num_iterations);
      return;
    }
  }
  // TODO how to respond to bad initialization
  // TODO add a search-based method to initialize if other methods failed
  // determine if the subset is a blocker and if so, force it to use simplex method:
  // also force simplex if it is a blocked subset (not enough speckles to use grad-based method)
  bool force_simplex = false;
  if(force_simplex_!=Teuchos::null)
    if(force_simplex_->find(subset_gid)!=force_simplex_->end()) force_simplex=true;
  if(obstructing_subset_ids_!=Teuchos::null){
    if(obstructing_subset_ids_->find(subset_gid)!=obstructing_subset_ids_->end()){
      if(obstructing_subset_ids_->find(subset_gid)->second.size()>0){
        force_simplex = true;
        DEBUG_MSG("[PROC " << comm_->get_rank() << "] SUBSET " << subset_gid << " is a blocker or blocked subset, forcing simplex method for this subset.");
      }
    }
    std::map<int_t,std::vector<int_t> >::iterator blk_it = obstructing_subset_ids_->begin();
    std::map<int_t,std::vector<int_t> >::iterator blk_end = obstructing_subset_ids_->end();
    for(;blk_it!=blk_end;++blk_it){
      std::vector<int_t> * obst_ids = &blk_it->second;
      for(size_t i=0;i<obst_ids->size();++i){
        if((*obst_ids)[i]==subset_gid){
          force_simplex = true;
          DEBUG_MSG("[PROC " << comm_->get_rank() << "] SUBSET " << subset_gid << " is a blocking subset, forcing simplex method for this subset.");
        }
      }
    }
  }
  const scalar_t prev_u = local_field_value(subset_gid,DICe::DISPLACEMENT_X);
  const scalar_t prev_v = local_field_value(subset_gid,DICe::DISPLACEMENT_Y);
  const scalar_t prev_t = local_field_value(subset_gid,DICe::ROTATION_Z);
  //
  // perform the correlation
  //
  if(optimization_method_==DICe::SIMPLEX||optimization_method_==DICe::SIMPLEX_THEN_GRADIENT_BASED||force_simplex){
    try{
      corr_status = obj->computeUpdateRobust(deformation,num_iterations);
    }
    catch (std::logic_error &err) { //a non-graceful exception occurred
      corr_status = CORRELATION_FAILED_BY_EXCEPTION;
    };
  }
  else if(optimization_method_==DICe::GRADIENT_BASED||optimization_method_==DICe::GRADIENT_BASED_THEN_SIMPLEX){
    try{
      corr_status = obj->computeUpdateFast(deformation,num_iterations);
    }
    catch (std::logic_error &err) { //a non-graceful exception occurred
      corr_status = CORRELATION_FAILED_BY_EXCEPTION;
    };
  }
  //
  //  test for the jump tolerances here:
  //
  // test for jump failure (too high displacement or rotation from last step due to subset getting lost)
  bool jump_pass = true;
  scalar_t diffU = ((*deformation)[DISPLACEMENT_X] - prev_u);
  scalar_t diffV = ((*deformation)[DISPLACEMENT_Y] - prev_v);
  scalar_t diffT = ((*deformation)[ROTATION_Z] - prev_t);
  DEBUG_MSG("Subset " << subset_gid << " U jump: " << diffU << " V jump: " << diffV << " T jump: " << diffT);
  if(std::abs(diffU) > disp_jump_tol_ || std::abs(diffV) > disp_jump_tol_ || std::abs(diffT) > theta_jump_tol_)
    jump_pass = false;
  DEBUG_MSG("Subset " << subset_gid << " jump pass: " << jump_pass);
  if(corr_status!=CORRELATION_SUCCESSFUL||!jump_pass){
    if(optimization_method_==DICe::SIMPLEX||optimization_method_==DICe::GRADIENT_BASED||force_simplex){
      record_failed_step(subset_gid,static_cast<int_t>(corr_status),num_iterations);
      return;
    }
    else if(optimization_method_==DICe::GRADIENT_BASED_THEN_SIMPLEX){
      // try again using simplex
      init_status = initial_guess(subset_gid,deformation);
      try{
        corr_status = obj->computeUpdateRobust(deformation,num_iterations);
      }
      catch (std::logic_error &err) { //a non-graceful exception occurred
        corr_status = CORRELATION_FAILED_BY_EXCEPTION;
      };
      if(corr_status!=CORRELATION_SUCCESSFUL){
        record_failed_step(subset_gid,static_cast<int_t>(corr_status),num_iterations);
        return;
      }
    }
    else if(optimization_method_==DICe::SIMPLEX_THEN_GRADIENT_BASED){
      // try again using gradient based
      init_status = initial_guess(subset_gid,deformation);
      try{
          corr_status = obj->computeUpdateFast(deformation,num_iterations);
      }
      catch (std::logic_error &err) { //a non-graceful exception occurred
        corr_status = CORRELATION_FAILED_BY_EXCEPTION;
      };
      if(corr_status!=CORRELATION_SUCCESSFUL){
        record_failed_step(subset_gid,static_cast<int_t>(corr_status),num_iterations);
        return;
      }
    }
  }
  //
  //  test final gamma if user requested
  //
  scalar_t noise_std_dev = 0.0;
  const scalar_t sigma = obj->sigma(deformation,noise_std_dev);
  if(sigma < 0.0){
    DEBUG_MSG("Subset " << subset_gid << " final sigma value FAILS threshold test, sigma: " <<
      sigma << " (threshold: " << 0.0 << ")");
    // TODO for the phase correlation initialization method, the initial guess needs to be stored
    record_failed_step(subset_gid,static_cast<int_t>(FRAME_FAILED_DUE_TO_NEGATIVE_SIGMA),num_iterations);
    return;
  }
  const scalar_t gamma = obj->gamma(deformation);
  const scalar_t beta = output_beta_ ? obj->beta(deformation) : 0.0;
  if(final_gamma_threshold_!=-1.0&&gamma > final_gamma_threshold_){
    DEBUG_MSG("Subset " << subset_gid << " final gamma value FAILS threshold test, gamma: " <<
      gamma << " (threshold: " << final_gamma_threshold_ << ")");
    // TODO for the phase correlation initialization method, the initial guess needs to be stored
    record_failed_step(subset_gid,static_cast<int_t>(FRAME_FAILED_DUE_TO_HIGH_GAMMA),num_iterations);
    return;
  }
  //
  //  test path distance if user requested
  //
  const bool has_path_file = path_file_names_->find(subset_gid)!=path_file_names_->end();
  if(path_distance_threshold_!=-1.0&&has_path_file){
    scalar_t path_distance = 0.0;
    size_t id = 0;
    // dynamic cast the pointer to get access to the derived class methods
    Teuchos::RCP<Path_Initializer> path_initializer =
        Teuchos::rcp_dynamic_cast<Path_Initializer>(opt_initializers_.find(subset_gid)->second);
    path_initializer->closest_triad((*deformation)[DISPLACEMENT_X],
      (*deformation)[DISPLACEMENT_Y],(*deformation)[ROTATION_Z],id,path_distance);
    DEBUG_MSG("Subset " << subset_gid << " path distance: " << path_distance);
    if(path_distance > path_distance_threshold_)
    {
      DEBUG_MSG("Subset " << subset_gid << " path distance value FAILS threshold test, distance from path: " <<
        path_distance << " (threshold: " << path_distance_threshold_ << ")");
      record_failed_step(subset_gid,static_cast<int_t>(FRAME_FAILED_DUE_TO_HIGH_PATH_DISTANCE),num_iterations);
      return;
    }
  }
  //
  //  Test jumps again
  //
  diffU = ((*deformation)[DISPLACEMENT_X] - prev_u);
  diffV = ((*deformation)[DISPLACEMENT_Y] - prev_v);
  diffT = ((*deformation)[ROTATION_Z] - prev_t);
  DEBUG_MSG("Subset " << subset_gid << " U jump: " << diffU << " V jump: " << diffV << " T jump: " << diffT);
  if(std::abs(diffU) > disp_jump_tol_ || std::abs(diffV) > disp_jump_tol_ || std::abs(diffT) > theta_jump_tol_){
    DEBUG_MSG("Subset " << subset_gid << " FAILS jump test: ");
    // TODO for the phase correlation initialization method, the initial guess needs to be stored
    record_failed_step(subset_gid,static_cast<int_t>(JUMP_TOLERANCE_EXCEEDED),num_iterations);
    return;
  }
  // TODO how to respond to failure here? or for initialization?
  //
  // SUCCESS
  //
  if(projection_method_==VELOCITY_BASED) save_off_fields(subset_gid);
  const scalar_t contrast = obj->subset()->contrast_std_dev();
  const int_t active_pixels = obj->subset()->num_active_pixels();
  record_step(subset_gid,
    deformation,sigma,0.0,gamma,beta,noise_std_dev,contrast,active_pixels,
    static_cast<int_t>(init_status),num_iterations);
  //
  //  turn on pixels that at the beginning were hidden behind an obstruction
  //
  if(use_subset_evolution_&&image_frame_>1){
    DEBUG_MSG("[PROC " << comm_->get_rank() << "] Evolving subset " << subset_gid << " using newly exposed pixels for intensity values");
    obj->subset()->turn_on_previously_obstructed_pixels();
  }
  //
  //  Write debugging images if requested
  //
  if(output_deformed_subset_intensity_images_)
    write_deformed_subset_intensity_image(obj);
  if(output_evolved_subset_images_)
    write_reference_subset_intensity_image(obj);
}

void
Schema::write_deformed_subset_intensity_image(Teuchos::RCP<Objective> obj){
#ifndef DICE_DISABLE_BOOST_FILESYSTEM
    DEBUG_MSG("[PROC " << comm_->get_rank() << "] Attempting to create directory : ./deformed_subset_intensities/");
    std::string dirStr = "./deformed_subset_intensities/";
    boost::filesystem::path dir(dirStr);
    if(boost::filesystem::create_directory(dir)) {
      DEBUG_MSG("[PROC " << comm_->get_rank() << "] Directory successfully created");
    }
    int_t num_zeros = 0;
    if(num_image_frames_>0){
      int_t num_digits_total = 0;
      int_t num_digits_image = 0;
      int_t decrement_total = num_image_frames_;
      int_t decrement_image = image_frame_;
      while (decrement_total){decrement_total /= 10; num_digits_total++;}
      if(image_frame_==0) num_digits_image = 1;
      else
        while (decrement_image){decrement_image /= 10; num_digits_image++;}
      num_zeros = num_digits_total - num_digits_image;
    }
    std::stringstream ss;
    ss << dirStr << "deformedSubset_" << obj->correlation_point_global_id() << "_";
    for(int_t i=0;i<num_zeros;++i)
      ss << "0";
    ss << image_frame_ << ".tif";
    obj->subset()->write_tiff(ss.str(),true);
#endif
}

void
Schema::write_reference_subset_intensity_image(Teuchos::RCP<Objective> obj){
#ifndef DICE_DISABLE_BOOST_FILESYSTEM
    DEBUG_MSG("[PROC " << comm_->get_rank() << "] Attempting to create directory : ./evolved_subsets/");
    std::string dirStr = "./evolved_subsets/";
    boost::filesystem::path dir(dirStr);
    if(boost::filesystem::create_directory(dir)) {
      DEBUG_MSG("[PROC " << comm_->get_rank() << "[ Directory successfully created");
    }
    int_t num_zeros = 0;
    if(num_image_frames_>0){
      int_t num_digits_total = 0;
      int_t num_digits_image = 0;
      int_t decrement_total = num_image_frames_;
      int_t decrement_image = image_frame_;
      while (decrement_total){decrement_total /= 10; num_digits_total++;}
      if(image_frame_==0) num_digits_image = 1;
      else
        while (decrement_image){decrement_image /= 10; num_digits_image++;}
      num_zeros = num_digits_total - num_digits_image;
    }
    std::stringstream ss;
    ss << dirStr << "evolvedSubset_" << obj->correlation_point_global_id() << "_";
    for(int_t i=0;i<num_zeros;++i)
      ss << "0";
    ss << image_frame_ << ".tif";
    obj->subset()->write_tiff(ss.str());
#endif
}

// TODO fix this up so that it works with conformal subsets:
void
Schema::write_control_points_image(const std::string & fileName,
  const bool use_def_image,
  const bool use_one_point){

  assert(subset_dim_>0);
  Teuchos::RCP<Image> img = (use_def_image) ? def_imgs_[0] : ref_img_;

  const int_t width = img->width();
  const int_t height = img->height();

  // first, create new intensities based on the old
  Teuchos::ArrayRCP<intensity_t> intensities(width*height,0.0);
  for (int_t i=0;i<width*height;++i)
    intensities[i] = (*img)(i);

  int_t x=0,y=0,xAlt=0,yAlt=0;
  const int_t numLocalControlPts = data_num_points_; //cp_map_->getNodeNumElements();
  // put a black box around the subset
  int_t i_start = 0;
  if(use_one_point) i_start = numLocalControlPts/2;
  const int_t i_end = use_one_point ? i_start + 1 : numLocalControlPts;
  const int_t color = use_one_point ? 255 : 0;
  for (int_t i=i_start;i<i_end;++i){
    x = field_value(i,COORDINATE_X);
    y = field_value(i,COORDINATE_Y);
    for(int_t j=0;j<subset_dim_;++j){
      xAlt = x - subset_dim_/2 + j;
      intensities[(y+subset_dim_/2)*width+xAlt] = color;
      intensities[(y-subset_dim_/2)*width+xAlt] = color;
    }
    for(int_t j=0;j<subset_dim_;++j){
      yAlt = y - subset_dim_/2 + j;
      intensities[(x+subset_dim_/2)+width*yAlt] = color;
      intensities[(x-subset_dim_/2)+width*yAlt] = color;
    }
  }
  // place white plus signs at the control points
  for (int_t i=0;i<numLocalControlPts;++i){
    x = field_value(i,COORDINATE_X);
    y = field_value(i,COORDINATE_Y);
    intensities[y*width+x] = 255;
    for(int_t j=0;j<3;++j){
      intensities[y*width+(x+j)] = 255;
      intensities[y*width+(x-j)] = 255;
      intensities[(y+j)*width + x] = 255;
      intensities[(y-j)*width + x] = 255;
    }
  }
  // place black plus signs at the control points that were successful
  for (int_t i=0;i<numLocalControlPts;++i){
    if(field_value(i,SIGMA)<=0) return;
    x = field_value(i,COORDINATE_X);
    y = field_value(i,COORDINATE_Y);
    intensities[y*width+x] = 0;
    for(int_t j=0;j<2;++j){
      intensities[y*width+(x+j)] = 0;
      intensities[y*width+(x-j)] = 0;
      intensities[(y+j)*width + x] = 0;
      intensities[(y-j)*width + x] = 0;
    }
  }
  // create a new image based on the info above:
  Teuchos::RCP<Image> new_img = Teuchos::rcp(new Image(width,height,intensities));

  // write the image:
  new_img->write(fileName);
}

void
Schema::write_output(const std::string & output_folder,
  const std::string & prefix,
  const bool separate_files_per_subset,
  const bool separate_header_file,
  const Output_File_Type type){
//  assert(analysis_type_!=CONSTRAINED_OPT && "Error, writing output from a schema using constrained optimization is not enabled.");
//  assert(analysis_type_!=INTEGRATED_DIC && "Error, writing output from a schema using integrated DIC is not enabled.");
  int_t my_proc = comm_->get_rank();
  if(my_proc!=0) return;
  int_t proc_size = comm_->get_size();

  TEUCHOS_TEST_FOR_EXCEPTION(type!=TEXT_FILE,std::invalid_argument,
    "Currently only TEXT_FILE output is implemented");
  TEUCHOS_TEST_FOR_EXCEPTION(output_spec_==Teuchos::null,std::runtime_error,"");

  std::stringstream infoName;
  infoName << output_folder << prefix << ".info";

  if(separate_files_per_subset){
    for(int_t subset=0;subset<data_num_points_;++subset){
      // determine the number of digits to append:
      int_t num_digits_total = 0;
      int_t num_digits_subset = 0;
      int_t decrement_total = data_num_points_;
      int_t decrement_subset = subset;
      while (decrement_total){decrement_total /= 10; num_digits_total++;}
      if(subset==0) num_digits_subset = 1;
      else
        while (decrement_subset){decrement_subset /= 10; num_digits_subset++;}
      int_t num_zeros = num_digits_total - num_digits_subset;

      // determine the file name for this subset
      std::stringstream fName;
      fName << output_folder << prefix << "_";
      for(int_t i=0;i<num_zeros;++i)
        fName << "0";
      fName << subset;
      if(proc_size>1)
        fName << "." << proc_size;
      fName << ".txt";
      if(image_frame_==1){
        std::FILE * filePtr = fopen(fName.str().c_str(),"w"); // overwrite the file if it exists
        if(separate_header_file){
          std::FILE * infoFilePtr = fopen(infoName.str().c_str(),"w"); // overwrite the file if it exists
          output_spec_->write_info(infoFilePtr);
          fclose(infoFilePtr);
         }
        else{
          output_spec_->write_info(filePtr);
        }
        output_spec_->write_header(filePtr,"FRAME");
        fclose (filePtr);
      }
      // append the latest result to the file
      std::FILE * filePtr = fopen(fName.str().c_str(),"a");
      output_spec_->write_frame(filePtr,first_frame_index_+image_frame_-1,subset);
      fclose (filePtr);
    } // subset loop
  }
  else{
    std::stringstream fName;
    // determine the number of digits to append:
    int_t num_zeros = 0;
    if(num_image_frames_>0){
      int_t num_digits_total = 0;
      int_t num_digits_image = 0;
      int_t decrement_total = num_image_frames_;
      int_t decrement_image = image_frame_ -1;
      while (decrement_total){decrement_total /= 10; num_digits_total++;}
      if(image_frame_-1==0) num_digits_image = 1;
      else
        while (decrement_image){decrement_image /= 10; num_digits_image++;}
      num_zeros = num_digits_total - num_digits_image;
    }
    fName << output_folder << prefix << "_";
    for(int_t i=0;i<num_zeros;++i)
      fName << "0";
    fName << image_frame_ - 1;
    if(proc_size >1)
      fName << "." << proc_size;
    fName << ".txt";
    std::FILE * filePtr = fopen(fName.str().c_str(),"w");
    if(separate_header_file){
      std::FILE * infoFilePtr = fopen(infoName.str().c_str(),"w"); // overwrite the file if it exists
      output_spec_->write_info(infoFilePtr);
      fclose(infoFilePtr);
     }
    else{
      output_spec_->write_info(filePtr);
    }
    output_spec_->write_header(filePtr,"SUBSET_ID");

    for(int_t i=0;i<data_num_points_;++i){
      output_spec_->write_frame(filePtr,i,i);
    }
    fclose (filePtr);
  }
}

void
Schema::print_fields(const std::string & fileName){

  if(data_num_points_==0){
    std::cout << " Schema has 0 control points." << std::endl;
    return;
  }
  if(fields_->get_num_fields()==0){
    std::cout << " Schema fields are emplty." << std::endl;
    return;
  }
  const int_t proc_id = comm_->get_rank();

  if(fileName==""){
    std::cout << "[PROC " << proc_id << "] DICE::Schema Fields and Values: " << std::endl;
    for(int_t i=0;i<data_num_points_;++i){
      std::cout << "[PROC " << proc_id << "] Control Point ID: " << i << std::endl;
      for(int_t j=0;j<DICe::MAX_FIELD_NAME;++j){
        std::cout << "[PROC " << proc_id << "]   " << to_string(static_cast<Field_Name>(j)) <<  " " <<
            field_value(i,static_cast<Field_Name>(j)) << std::endl;
        if(dist_map_->get_local_element(i)!=-1){
          std::cout << "[PROC " << proc_id << "]   " << to_string(static_cast<Field_Name>(j)) <<  " (has distributed value)  " <<
              local_field_value(i,static_cast<Field_Name>(j)) << std::endl;
        }
      }
    }

  }
  else{
    std::FILE * outFile;
    outFile = fopen(fileName.c_str(),"a");
    for(int_t i=0;i<data_num_points_;++i){
      fprintf(outFile,"%i ",i);
      for(int_t j=0;j<DICe::MAX_FIELD_NAME;++j){
        fprintf(outFile," %4.4E ",field_value(i,static_cast<Field_Name>(j)));
      }
      fprintf(outFile,"\n");
    }
    fclose(outFile);
  }
}

void
Schema::check_for_blocking_subsets(const int_t subset_global_id){
  if(obstructing_subset_ids_==Teuchos::null) return;
  if(obstructing_subset_ids_->find(subset_global_id)==obstructing_subset_ids_->end()) return;
  if(obstructing_subset_ids_->find(subset_global_id)->second.size()==0) return;

  const int_t subset_local_id = get_local_id(subset_global_id);

  // turn off pixels in subset 0 that are blocked by 1 and 2
  // get a pointer to the member data in the subset that will store the list of blocked pixels
  std::set<std::pair<int_t,int_t> > & blocked_pixels =
      *obj_vec_[subset_local_id]->subset()->pixels_blocked_by_other_subsets();
  blocked_pixels.clear();

  // get the list of subsets that block this one
  std::vector<int_t> * obst_ids = &obstructing_subset_ids_->find(subset_global_id)->second;
  // iterate over all the blocking subsets
  for(size_t si=0;si<obst_ids->size();++si){
    int_t global_ss = (*obst_ids)[si];
    int_t local_ss = get_local_id(global_ss);
    assert(local_ss>=0);
    int_t cx = obj_vec_[local_ss]->subset()->centroid_x();
    int_t cy = obj_vec_[local_ss]->subset()->centroid_y();
    Teuchos::RCP<std::vector<scalar_t> > def = Teuchos::rcp(new std::vector<scalar_t>(DICE_DEFORMATION_SIZE,0.0));
    (*def)[DICe::DISPLACEMENT_X]  = local_field_value(global_ss,DICe::DISPLACEMENT_X);
    (*def)[DICe::DISPLACEMENT_Y]  = local_field_value(global_ss,DICe::DISPLACEMENT_Y);
    (*def)[DICe::ROTATION_Z]      = local_field_value(global_ss,DICe::ROTATION_Z);
    (*def)[DICe::NORMAL_STRAIN_X] = local_field_value(global_ss,DICe::NORMAL_STRAIN_X);
    (*def)[DICe::NORMAL_STRAIN_Y] = local_field_value(global_ss,DICe::NORMAL_STRAIN_Y);
    (*def)[DICe::SHEAR_STRAIN_XY] = local_field_value(global_ss,DICe::SHEAR_STRAIN_XY);
    std::set<std::pair<int_t,int_t> > subset_pixels =
        obj_vec_[local_ss]->subset()->deformed_shapes(def,cx,cy,obstruction_skin_factor_);
    blocked_pixels.insert(subset_pixels.begin(),subset_pixels.end());
  } // blocking subsets loop
}

void
Schema::write_deformed_subsets_image(const bool use_gamma_as_color){
#ifndef DICE_DISABLE_BOOST_FILESYSTEM
  if(obj_vec_.empty()) return;
  // if the subset_images folder does not exist, create it
  // TODO allow user to specify where this goes
  // If the dir is already there this step becomes a no-op
  DEBUG_MSG("Attempting to create directory : ./deformed_subsets/");
  std::string dirStr = "./deformed_subsets/";
  boost::filesystem::path dir(dirStr);
  if(boost::filesystem::create_directory(dir)) {
    DEBUG_MSG("Directory successfully created");
  }
  int_t num_zeros = 0;
  if(num_image_frames_>0){
    int_t num_digits_total = 0;
    int_t num_digits_image = 0;
    int_t decrement_total = num_image_frames_;
    int_t decrement_image = image_frame_;
    while (decrement_total){decrement_total /= 10; num_digits_total++;}
    if(image_frame_==0) num_digits_image = 1;
    else
      while (decrement_image){decrement_image /= 10; num_digits_image++;}
    num_zeros = num_digits_total - num_digits_image;
  }
  const int_t proc_id = comm_->get_rank();
  std::stringstream ss;
  ss << dirStr << "def_subsets_p_" << proc_id << "_";
  for(int_t i=0;i<num_zeros;++i)
    ss << "0";
  ss << image_frame_ << ".tif";

  const int_t w = ref_img_->width();
  const int_t h = ref_img_->height();

  Teuchos::ArrayRCP<intensity_t> intensities(w*h,0.0);

  // construct a copy of the base image to use as layer 0 for the output;
  // read each sub image if motion windows are used
  for(int_t sub=0;sub<(int_t)def_imgs_.size();++sub){
    if(def_imgs_[sub]==Teuchos::null)continue;
    const int_t offset_x = def_imgs_[sub]->offset_x();
    const int_t offset_y = def_imgs_[sub]->offset_y();
    for(int_t y=0;y<def_imgs_[sub]->height();++y){
      for(int_t x=0;x<def_imgs_[sub]->width();++x){
        intensities[(y+offset_y)*w + x + offset_x] = (*def_imgs_[sub])(x,y);
      }
    }
  }

  scalar_t dx=0,dy=0;
  int_t ox=0,oy=0;
  int_t Dx=0,Dy=0;
  scalar_t X=0.0,Y=0.0;
  int_t px=0,py=0;

  // create output for each subset
  //for(int_t subset=0;subset<1;++subset){
  for(size_t subset=0;subset<obj_vec_.size();++subset){
    const int_t gid = obj_vec_[subset]->correlation_point_global_id();

    //if(gid==1) continue;
    // get the deformation vector for each subset
    const scalar_t u     = local_field_value(gid,DICe::DISPLACEMENT_X);
    const scalar_t v     = local_field_value(gid,DICe::DISPLACEMENT_Y);
    const scalar_t theta = local_field_value(gid,DICe::ROTATION_Z);
    const scalar_t dudx  = local_field_value(gid,DICe::NORMAL_STRAIN_X);
    const scalar_t dvdy  = local_field_value(gid,DICe::NORMAL_STRAIN_Y);
    const scalar_t gxy   = local_field_value(gid,DICe::SHEAR_STRAIN_XY);
    DEBUG_MSG("Write deformed subset " << gid << " u " << u << " v " << v << " theta " << theta << " dudx " << dudx << " dvdy " << dvdy << " gxy " << gxy);
    Teuchos::RCP<DICe::Subset> ref_subset = obj_vec_[subset]->subset();
    ox = ref_subset->centroid_x();
    oy = ref_subset->centroid_y();
    scalar_t mean_sum_ref = 0.0;
    scalar_t mean_sum_def = 0.0;
    scalar_t mean_ref = 0.0;
    scalar_t mean_def = 0.0;
    if(use_gamma_as_color){
      mean_ref = obj_vec_[subset]->subset()->mean(REF_INTENSITIES,mean_sum_ref);
      mean_def = obj_vec_[subset]->subset()->mean(DEF_INTENSITIES,mean_sum_def);
      TEUCHOS_TEST_FOR_EXCEPTION(mean_sum_ref==0.0||mean_sum_def==0.0,std::runtime_error," invalid mean sum (cannot be 0.0, ZNSSD is then undefined)" <<
        mean_sum_ref << " " << mean_sum_def);
    }
    // loop over each pixel in the subset
    scalar_t pixel_gamma = 0.0;
    for(int_t i=0;i<ref_subset->num_pixels();++i){
      dx = ref_subset->x(i) - ox;
      dy = ref_subset->y(i) - oy;
      // stretch and shear the coordinate
      Dx = (1.0+dudx)*dx + gxy*dy;
      Dy = (1.0+dvdy)*dy + gxy*dx;
      //  Rotation                                  // translation // convert to global coordinates
      X = std::cos(theta)*Dx - std::sin(theta)*Dy + u            + ox;
      Y = std::sin(theta)*Dx + std::cos(theta)*Dy + v            + oy;
      // get the nearest pixel location:
      px = (int_t)X;
      if(X - (int_t)X >= 0.5) px++;
      py = (int_t)Y;
      if(Y - (int_t)Y >= 0.5) py++;
      // offset the pixel locations by the sub image offsets
      if(px>=0&&px<w&&py>=0&&py<h){
        if(use_gamma_as_color){
          if(ref_subset->is_active(i)&!ref_subset->is_deactivated_this_step(i)){
            pixel_gamma =  (ref_subset->def_intensities(i)-mean_def)/mean_sum_def - (ref_subset->ref_intensities(i)-mean_ref)/mean_sum_ref;
            intensities[py*w+px] = pixel_gamma*pixel_gamma*10000.0;
          }
        }else{
          if(!ref_subset->is_active(i)){
            intensities[py*w+px] = 75;
          }
          else{
            // color shows correlation quality
            intensities[py*w+px] = 100;//ref_subset->per_pixel_gamma(i)*85000;
          }
          // trun all deactivated pixels white
          if(ref_subset->is_deactivated_this_step(i)){
            intensities[py*w+px] = 255;
          }
        } // not use_gamma_as_color
      } // range guard
    } // pixel loop

  } // subset loop

  Teuchos::RCP<Image> layer_0_image = Teuchos::rcp(new Image(w,h,intensities));
  layer_0_image->write(ss.str());
#else
  DEBUG_MSG("Warning, write_deformed_image() was called, but Boost::filesystem is not enabled making this a no-op.");
#endif
}


int_t
Schema::strain_window_size(const int_t post_processor_index)const{
  assert((int_t)post_processors_.size()>post_processor_index);
    return post_processors_[post_processor_index]->strain_window_size();
}

Output_Spec::Output_Spec(Schema * schema,
  const bool omit_row_id,
  const Teuchos::RCP<Teuchos::ParameterList> & params,
  const std::string & delimiter):
  schema_(schema),
  delimiter_(delimiter), // space delimited is default
  omit_row_id_(omit_row_id)
{

  // default output format
  if(params == Teuchos::null){
    field_names_.push_back(to_string(DICe::COORDINATE_X));
    post_processor_ids_.push_back(-1);
    field_names_.push_back(to_string(DICe::COORDINATE_Y));
    post_processor_ids_.push_back(-1);
    field_names_.push_back(to_string(DICe::DISPLACEMENT_X));
    post_processor_ids_.push_back(-1);
    field_names_.push_back(to_string(DICe::DISPLACEMENT_Y));
    post_processor_ids_.push_back(-1);
    field_names_.push_back(to_string(DICe::ROTATION_Z));
    post_processor_ids_.push_back(-1);
    field_names_.push_back(to_string(DICe::NORMAL_STRAIN_X));
    post_processor_ids_.push_back(-1);
    field_names_.push_back(to_string(DICe::NORMAL_STRAIN_Y));
    post_processor_ids_.push_back(-1);
    field_names_.push_back(to_string(DICe::SHEAR_STRAIN_XY));
    post_processor_ids_.push_back(-1);
    field_names_.push_back(to_string(DICe::SIGMA));
    post_processor_ids_.push_back(-1);
    field_names_.push_back(to_string(DICe::STATUS_FLAG));
    post_processor_ids_.push_back(-1);
  }
  else{
    // get the total number of field names
    int_t num_names = 0;
    // if the names are listed with bool values, count the number of true bools
    for(Teuchos::ParameterList::ConstIterator it=params->begin();it!=params->end();++it){
      std::string string_field_name = it->first;
      if(params->isType<bool>(string_field_name)){
        if(params->get<bool>(string_field_name,false))
          num_names++;
      }
      else if(params->isType<int_t>(string_field_name)){
        num_names++;
      }
      else{
        TEUCHOS_TEST_FOR_EXCEPTION(true,std::invalid_argument,"Error, output spec has incorrect syntax.");
      }
    }
    DEBUG_MSG("Output spec has " << num_names << " active fields");
    // get the max index
    field_names_.resize(num_names);
    post_processor_ids_.resize(num_names);
    int_t max_index = 0;
    std::set<int_t> indices;

    // read in the names and indices by iterating the parameter list
    int_t current_position = 0;
    for(Teuchos::ParameterList::ConstIterator it=params->begin();it!=params->end();++it){
      std::string string_field_name = it->first;
      stringToUpper(string_field_name);
      int_t post_processor_id = -1;
      bool paramValid = false;
      for(int_t j=0;j<MAX_FIELD_NAME;++j){
        if(string_field_name==to_string(static_cast<Field_Name>(j)))
          paramValid = true;
      }
      // see if this field is in one of the post processors instead
      for(int_t j=0;j<(int_t)schema_->post_processors()->size();++j){
        for(int_t k=0;k<(int_t)(*schema_->post_processors())[j]->field_names()->size();++k){
          if(string_field_name==(*(*schema_->post_processors())[j]->field_names())[k]){
            paramValid = true;
            post_processor_id = j;
          }
        }
      }
      if(!paramValid){
        std::cout << "Error: invalid field name requested in output spec: " << string_field_name << std::endl;
        TEUCHOS_TEST_FOR_EXCEPTION(true,std::runtime_error,"");
      }
      // check if the output spec was specified as integer values or bools
      int_t field_index = -1;
      if(params->isType<bool>(string_field_name)){
        if(params->get<bool>(string_field_name,false)){
          field_index = current_position;
          current_position++;
        }
        else
          continue;
      }
      else{
        field_index = params->get<int_t>(string_field_name);
      }
      DEBUG_MSG("Adding output field " << string_field_name << " in column " << field_index);
      if(field_index>num_names-1||field_index<0){
        std::cout << "Error: field index in output spec is invalid " << field_index << std::endl;
        TEUCHOS_TEST_FOR_EXCEPTION(true,std::runtime_error,"");
      }
      // see if this index exists already
      if(indices.find(field_index)!=indices.end()){
        std::cout << "Error: same field index assigned to multiple fields in output spec: " << field_index << std::endl;
        TEUCHOS_TEST_FOR_EXCEPTION(true,std::runtime_error,"");
      }
      indices.insert(field_index);
      if(field_index > max_index) max_index = field_index;
      field_names_[field_index] = string_field_name;
      post_processor_ids_[field_index] = post_processor_id;
    } // loop over field names in the parameterlist
    if(max_index!=num_names-1){
      std::cout << "Error: The max field index in the output spec is not equal to the number of fields, num_fields " << field_names_.size() << " max_index " << max_index << std::endl;
      TEUCHOS_TEST_FOR_EXCEPTION(true,std::runtime_error,"");
    }
  }
};

void
Output_Spec::write_info(std::FILE * file){
  assert(file);
  fprintf(file,"***\n");
  fprintf(file,"*** Digital Image Correlation Engine (DICe), (git sha1: %s) Copyright 2015 Sandia Corporation\n",GITSHA1);
  fprintf(file,"***\n");
  fprintf(file,"*** Reference image: %s \n",schema_->ref_img()->file_name().c_str());
  fprintf(file,"*** Deformed image: %s \n",schema_->def_img(0)->file_name().c_str());
  if(schema_->analysis_type()==GLOBAL_DIC){
    fprintf(file,"*** DIC method : global \n");
  }
  else{
    fprintf(file,"*** DIC method : local \n");
  }
  fprintf(file,"*** Correlation method: ZNSSD\n");
  std::string interp_method = to_string(schema_->interpolation_method());
  fprintf(file,"*** Interpolation method: %s\n",interp_method.c_str());
  fprintf(file,"*** Image gradient method: FINITE_DIFFERENCE\n");
  std::string opt_method = to_string(schema_->optimization_method());
  fprintf(file,"*** Optimization method: %s\n",opt_method.c_str());
  std::string proj_method = to_string(schema_->projection_method());
  fprintf(file,"*** Projection method: %s\n",proj_method.c_str());
  std::string init_method = to_string(schema_->initialization_method());
  fprintf(file,"*** Guess initialization method: %s\n",init_method.c_str());
  fprintf(file,"*** Seed location: N/A\n");
  fprintf(file,"*** Shape functions: ");
  if(schema_->translation_enabled())
    fprintf(file,"Translation (u,v) ");
  if(schema_->rotation_enabled())
    fprintf(file,"Rotation (theta) ");
  if(schema_->normal_strain_enabled())
    fprintf(file,"Normal Strain (ex,ey) ");
  if(schema_->shear_strain_enabled())
    fprintf(file,"Shear Strain (gamma_xy) ");
  fprintf(file,"\n");
  fprintf(file,"*** Incremental correlation: false\n");
  if(schema_->analysis_type()==GLOBAL_DIC){
    fprintf(file,"*** Mesh size: %i\n",schema_->mesh_size());
    fprintf(file,"*** Step size: N/A\n");
  }
  else{
    fprintf(file,"*** Subset size: %i\n",schema_->subset_dim());
    fprintf(file,"*** Step size: x %i y %i (-1 implies not regular grid)\n",schema_->step_size_x(),schema_->step_size_y());
  }
  if(schema_->post_processors()->size()==0)
    fprintf(file,"*** Strain window: N/A\n");
  else
    fprintf(file,"*** Strain window size in pixels: %i (only first strain post-processor is reported)\n",schema_->strain_window_size(0));
  fprintf(file,"*** Coordinates given with (0,0) as upper left corner of image, x positive right, y positive down\n");
  fprintf(file,"***\n");
}

void
Output_Spec::write_header(std::FILE * file,
  const std::string & row_id){
  assert(file);
  if(!omit_row_id_)
    fprintf(file,"%s%s",row_id.c_str(),delimiter_.c_str());
  for(size_t i=0;i<field_names_.size();++i){
    if(i==0)
      fprintf(file,"%s",field_names_[i].c_str());
    else
      fprintf(file,"%s%s",delimiter_.c_str(),field_names_[i].c_str());
  }
  fprintf(file,"\n");
}

void
Output_Spec::write_frame(std::FILE * file,
  const int_t row_index,
  const int_t field_value_index){

  assert(file);
  if(!omit_row_id_)
    fprintf(file,"%i%s",row_index,delimiter_.c_str());
  assert(field_names_.size()==post_processor_ids_.size());
  for(size_t i=0;i<field_names_.size();++i)
  {
    // if the field_name is from one of the schema fields, get the information from the schema
    scalar_t value = 0.0;
    if(post_processor_ids_[i]==-1)
      value = schema_->field_value(field_value_index,string_to_field_name(field_names_[i]));
    // otherwise the field must belong to a post processor
    else{
      assert(post_processor_ids_[i]>=0 && post_processor_ids_[i]<(int_t)schema_->post_processors()->size());
      value = (*schema_->post_processors())[post_processor_ids_[i]]->field_value(field_value_index,field_names_[i]);
    }
    if(i==0)
      fprintf(file,"%4.4E",value);
    else
      fprintf(file,"%s%4.4E",delimiter_.c_str(),value);
  }
  fprintf(file,"\n"); // the space before end of line is important for parsing in the output diff tool
}

bool frame_should_be_skipped(const int_t trigger_based_frame_index,
  std::vector<int_t> & frame_id_vector){
  DEBUG_MSG("frame_should_be_skipped(): vector size " << frame_id_vector.size());
  if(frame_id_vector.size()==0) return true;

  int_t index = frame_id_vector.size();
  for(size_t i=0;i<frame_id_vector.size();++i){
    if(trigger_based_frame_index < frame_id_vector[i]){
      index = i;
      break;
    }
  }
  DEBUG_MSG("frame_should_be_skipped(): index " << index);
  if(index%2==0||index==0) return false;
  else return true;
}

}// End DICe Namespace
