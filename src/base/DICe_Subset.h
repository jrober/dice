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

#ifndef DICE_SUBSET_H
#define DICE_SUBSET_H

#include <DICe.h>
#include <DICe_Shape.h>
#include <DICe_Image.h>
#if DICE_KOKKOS
  #include <DICe_Kokkos.h>
#endif

#include <Teuchos_ArrayRCP.hpp>

/*!
 *  \namespace DICe
 *  @{
 */
/// generic DICe classes and functions
namespace DICe {

/// \class DICe::Subset
/// \brief Subsets are used to store temporary collections of pixels for comparison between the
/// reference and deformed images. The data that is stored by a subset is a list of x and y
/// corrdinates of each pixel (this allows for arbitrary shape) and containers for pixel
/// intensity values.

class DICE_LIB_DLL_EXPORT
Subset {
public:

  /// constructor that takes arrays of the pixel locations as input
  /// (note this type of subset has not been enabled for obstruction detection,
  /// only conformal subsets with defined shapes have this enabled. See the
  /// constructor below with a subset_def argument)
  /// \param cx centroid x pixel location
  /// \param cy centroid y pixel location
  /// \param x array of x coordinates
  /// \param y array of y coordinates
  Subset(int_t cx,
    int_t cy,
    Teuchos::ArrayRCP<int_t> x,
    Teuchos::ArrayRCP<int_t> y);

  /// constructor that takes a centroid and dims as arguments
  /// (note this type of subset has not been enabled for obstruction detection,
  /// only conformal subsets with defined shapes have this enabled. See the
  /// constructor below with a subset_def argument)
  /// \param cx centroid x pixel location
  /// \param cy centroid y pixel location
  /// \param width width of the centroid (should be an odd number, otherwise the next larger odd number is used)
  /// \param height height of the centroid (should be an odd number, otherwise the next larger odd numer is used)
  Subset(const int_t cx,
    const int_t cy,
    const int_t width,
    const int_t height);

  /// constructor that takes a conformal subset def as the input
  /// \param cx centroid x pixel location
  /// \param cy centroid y pixel location
  /// \param subset_def the definition of the subset areas
  Subset(const int_t cx,
    const int_t cy,
    const Conformal_Area_Def & subset_def);

  /// virtual destructor
  virtual ~Subset(){};

  /// returns the number of pixels in the subset
  int_t num_pixels()const{
    return num_pixels_;
  }

  /// returns the centroid x pixel location
  int_t centroid_x()const{
    return cx_;
  }

  /// returns the centroid y pixel location
  int_t centroid_y()const{
    return cy_;
  }

  /// x coordinate accessor
  /// \param pixel_index the pixel id
  /// note there is no bounds checking on the index
  const int_t& x(const int_t pixel_index)const;

  /// y coordinate accessor
  /// \param pixel_index the pixel id
  /// note there is no bounds checking on the index
  const int_t& y(const int_t pixel_index)const;

  /// gradient x accessor
  /// \param pixel_index the pixel id
  /// note there is no bounds checking
  const scalar_t& grad_x(const int_t pixel_index)const;

  /// gradient y accessor
  /// \param pixel_index the pixel id
  /// note there is no bounds checking
  const scalar_t& grad_y(const int_t pixel_index)const;

  /// returns true if the gradients have been computed
  bool has_gradients()const{
    return has_gradients_;
  }

  /// returns a copy of the gradient x values as an array
  Teuchos::ArrayRCP<scalar_t> grad_x_array()const;

  /// returns a copy of the gradient x values as an array
  Teuchos::ArrayRCP<scalar_t> grad_y_array()const;

  /// ref intensities accessor
  /// \param pixel_index the pixel id
  intensity_t& ref_intensities(const int_t pixel_index);

  /// ref intensities accessor
  /// \param pixel_index the pixel id
  intensity_t& def_intensities(const int_t pixel_index);

  /// initialization method:
  /// \param image the image to get the intensity values from
  /// \param target the initialization mode (put the values in the ref or def intensities)
  /// \param deformation the deformation map (optional)
  /// \param interp interpolation method (optional)
  void initialize(Teuchos::RCP<Image> image,
    const Subset_View_Target target=REF_INTENSITIES,
    Teuchos::RCP<const std::vector<scalar_t> > deformation=Teuchos::null,
    const Interpolation_Method interp=KEYS_FOURTH);

  /// write the subset intensity values to a tif file
  /// \param file_name the name of the tif file to write
  /// \param use_def_intensities use the deformed intensities rather than the reference
  void write_tiff(const std::string & file_name,
    const bool use_def_intensities=false);

  /// draw the subset over an image
  /// \param file_name the name of the tif file to write
  /// \param image pointer to the image to use as the background
  /// \param deformation deform the subset on the image (rather than its original shape)
  void write_subset_on_image(const std::string & file_name,
    Teuchos::RCP<Image> image,
    Teuchos::RCP<const std::vector<scalar_t> > deformation=Teuchos::null);

  /// returns the mean intensity value
  /// \param target either the reference or deformed intensity values
  scalar_t mean(const Subset_View_Target target);

  /// returns the mean intensity value
  /// \param target either the reference or deformed intensity values
  /// \param sum [output] returns the reduction value of the intensities minus the mean
  scalar_t mean(const Subset_View_Target target,
    scalar_t & sum);

  /// returns the ZNSSD gamma correlation value between the reference and deformed subsets
  scalar_t gamma();

  /// reset the is_active bool for each pixel to true;
  void reset_is_active();

  /// returns true if this pixel is active
  bool & is_active(const int_t pixel_index);

  /// returns the number of active pixels in the subset
  int_t num_active_pixels();

  /// returns true if this pixel is deactivated for this particular frame
  bool & is_deactivated_this_step(const int_t pixel_index);

  /// reset the is_deactivated_this_step bool for each pixel to false
  void reset_is_deactivated_this_step();

  /// True if the subset is not square and has geometry information in a Conformal_Area_Def
  bool is_conformal()const{
    return is_conformal_;
  }

  /// Returns a pointer to the subset's Conformal_Area_Def
  const Conformal_Area_Def * conformal_subset_def()const{
    return &conformal_subset_def_;
  }

  /// Return the id of the sub-region of a global image to use
  int_t sub_image_id()const{
    return sub_image_id_;
  }

  /// set the sub_image_id
  void set_sub_image_id(const int_t id){
    sub_image_id_ = id;
  }

  /// \brief Returns an estimate of the noise standard deviation for this subset based on the method
  /// of J. Immerkaer, Fast Noise Variance Estimation, Computer Vision and
  /// Image Understanding, Vol. 64, No. 2, pp. 300-302, Sep. 1996
  /// The estimate is computed for a rectangular window that encompases the entire subset if the subset is conformal
  /// \param image the image for which to estimate the noise for this subset
  /// \param deformation the current deformation of the subset
  scalar_t noise_std_dev(Teuchos::RCP<Image> image,
    Teuchos::RCP<const std::vector<scalar_t> > deformation);

  /// \brief Returns the std deviation of the image intensity values
  scalar_t contrast_std_dev();

  /// \brief EXPERIMENTAL Check the deformed position of the pixel to see if it falls inside an obstruction, if so, turn it off
  /// \param deformation Deformation to use in determining the current position of all the pixels in the subset
  ///
  /// This method uses the specified deformation vector to deform the subset to the current position. It then
  /// checks to see if any of the deformed pixels fall behind an obstruction. These obstructed pixels
  /// are turned off by setting the is_deactivated_this_step_[i] flag to true. These flags are reset on every frame.
  /// When methods like gamma() are called on an objective, these pixels get skipped so they do not contribute to
  /// the correlation or the optimization routine.
  void turn_off_obstructed_pixels(Teuchos::RCP<const std::vector<scalar_t> > deformation);

  /// \brief EXPERIMENTAL See if any pixels that were obstructed to begin with are now in view, if so use the
  /// newly visible intensity values to reconstruct the subset
  ///
  /// Some pixels in the subset may be hidden in the first frame by an obstruction (or another subset that
  /// sits on top of this one). As the frames progress, these pixels eventually become visible. This method
  /// can be used to evolve the reference subset intensities by taking the intensity value of the pixel
  /// when it becomes visible. The location of the pixel in the current frame is not needed because it
  /// is assumed that the reference and deformed subset intensity arrays are ordered the same. So all that
  /// needs to be checked is if it was deactivated at frame zero, and is now not deactivated this step,
  /// that particular pixel can be used.
  void turn_on_previously_obstructed_pixels();

  /// \brief  EXPERIMENTAL Returns true if the given coordinates fall within an obstructed region for this subset
  /// \param coord_x global x-coordinate
  /// \param coord_y global y-coordinate
  bool is_obstructed_pixel(const scalar_t & coord_x,
    const scalar_t & coord_y)const;

  /// \brief EXPERIMENTAL Returns a pointer to the set of pixels currently obstructed by another subset
  std::set<std::pair<int_t,int_t> > * pixels_blocked_by_other_subsets(){
    return & pixels_blocked_by_other_subsets_;
  }

  /// \brief EXPERIMENTAL Return the deformed geometry information for the subset boundary
  std::set<std::pair<int_t,int_t> > deformed_shapes(Teuchos::RCP<const std::vector<scalar_t> > deformation=Teuchos::null,
    const int_t cx=0,
    const int_t cy=0,
    const scalar_t & skin_factor=1.0);

#if DICE_KOKKOS
  /// x coordinate view accessor
  pixel_coord_dual_view_1d x()const{
    return x_;
  }
  /// y coordinate view accessor
  pixel_coord_dual_view_1d y()const{
    return y_;
  }
  /// ref intensities device view accessor
  intensity_dual_view_1d ref_intensities()const{
    return ref_intensities_;
  }
  /// ref intensities device view accessor
  intensity_dual_view_1d def_intensities()const{
    return def_intensities_;
  }
#endif

private:
  /// number of pixels in the subset
  int_t num_pixels_;
#if DICE_KOKKOS
  /// pixel container
  intensity_dual_view_1d ref_intensities_;
  /// pixel container
  intensity_dual_view_1d def_intensities_;
  /// container for grad_x
  scalar_dual_view_1d grad_x_;
  /// container for grad_y
  scalar_dual_view_1d grad_y_;
  /// pixels can be deactivated by obstructions (persistent)
  bool_dual_view_1d is_active_;
  /// pixels can be deactivated for this frame only
  bool_dual_view_1d is_deactivated_this_step_;
  /// initial x position of the pixels in the reference image
  pixel_coord_dual_view_1d x_;
  /// initial x position of the pixels in the reference image
  pixel_coord_dual_view_1d y_;
#else
  /// pixel container
  Teuchos::ArrayRCP<intensity_t> ref_intensities_;
  /// pixel container
  Teuchos::ArrayRCP<intensity_t> def_intensities_;
  /// container for grad_x
  Teuchos::ArrayRCP<scalar_t> grad_x_;
  /// container for grad_y
  Teuchos::ArrayRCP<scalar_t> grad_y_;
  /// pixels can be deactivated by obstructions (persistent)
  Teuchos::ArrayRCP<bool> is_active_;
  /// pixels can be deactivated for this frame only
  Teuchos::ArrayRCP<bool> is_deactivated_this_step_;
  /// initial x position of the pixels in the reference image
  Teuchos::ArrayRCP<int_t> x_;
  /// initial x position of the pixels in the reference image
  Teuchos::ArrayRCP<int_t> y_;
#endif
  /// \brief EXPERIMENTAL Holds the obstruction coordinates if they exist.
  /// NOTE: The coordinates are switched for this (i.e. (Y,X)) so that
  /// the loops over y then x will be more efficient
  std::set<std::pair<int_t,int_t> > obstructed_coords_;
  /// \brief EXPERIMENTAL Holds the pixels blocked by other subsets if they exist.
  /// NOTE: The coordinates are switched for this (i.e. (Y,X)) so that
  /// the loops over y then x will be more efficient
  std::set<std::pair<int_t,int_t> > pixels_blocked_by_other_subsets_;
  /// centroid location x
  int_t cx_; // assumed to be the middle of the pixel
  /// centroid location y
  int_t cy_; // assumed to be the middle of the pixel
  /// true if the gradient values are populated
  bool has_gradients_;
  /// Conformal_Area_Def that defines the subset geometry
  Conformal_Area_Def conformal_subset_def_;
  /// The subset is not square
  bool is_conformal_;
  /// if sub regions of the frame are used instead of reading in the whole
  /// sub image, this sub_image_id defines which region to draw the pixel information from
  int_t sub_image_id_;
};

}// End DICe Namespace

/*! @} End of Doxygen namespace*/

#endif
