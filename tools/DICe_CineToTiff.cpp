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

/*! \file  DICe_CineToTiff.cpp
    \brief Utility for exporting cine files to tiff files
*/

#include <DICe.h>
#include <DICe_Parser.h>
#include <DICe_Cine.h>

#include <Teuchos_RCP.hpp>
#include <Teuchos_oblackholestream.hpp>

#include <cassert>

using namespace DICe;

int main(int argc, char *argv[]) {

  /// usage ./DICe_CineToTiff <cine_file_name> <start_index> <end_index> <output_prefix>

  DICe::initialize(argc, argv);

  //Teuchos::oblackholestream bhs; // outputs nothing
  Teuchos::RCP<std::ostream> outStream = Teuchos::rcp(&std::cout, false);
  std::string delimiter = " ,\r";

  if(argc==2){
    std::string help = argv[1];
    if(help=="-h"){
      std::cout << " DICe_CineToTiff (exports cine images to tiffs) " << std::endl;
      std::cout << " Syntax: DICe_CineToTiff <cine_file_name> <start_index (zero based)> "
          "<end_index (zero based)> <output_prefix> [rotation, (90,180,or270, other values ignored)]" << std::endl;
      exit(0);
    }
  }

  if(argc < 5) {
      printf("four input arguments are required <cine_file_name> <start_index> <end_index> <output_prefix>\n");
      exit(0);
  }

  DEBUG_MSG("User specified " << argc << " arguments");
  for(int_t i=0;i<argc;++i){
    DEBUG_MSG(argv[i]);
  }
  std::string fileName = argv[1];
  *outStream << "Cine file name: " << fileName << std::endl;
  std::string prefix = argv[4];
  *outStream << "Tiff prefix: " << prefix << std::endl;

  int_t rotation = 0;
  if(argc > 5){
    rotation = std::stoi(argv[5]);
    DEBUG_MSG("User requested image roation by " << rotation << " degrees");
  }

  Teuchos::RCP<DICe::cine::Cine_Reader> cine_reader  =  Teuchos::rcp(new DICe::cine::Cine_Reader(fileName,outStream.getRawPtr()));

  *outStream << "\nCine read successfully\n" << std::endl;

  const int_t num_images = cine_reader->num_frames();
  const int_t image_width = cine_reader->width();
  const int_t image_height = cine_reader->height();

  *outStream << "Num frames:     " << num_images << std::endl;
  *outStream << "Width:          " << image_width << std::endl;
  *outStream << "Height:         " << image_height << std::endl;

  const int_t start_frame = std::stoi(argv[2]);
  const int_t end_frame = std::stoi(argv[3]);
  assert(start_frame>=0);
  assert(start_frame<num_images);
  assert(end_frame>=start_frame);
  assert(end_frame<num_images);
  *outStream << "Start frame:    " << start_frame << std::endl;
  *outStream << "End frame:      " << end_frame << std::endl;

  for(int_t i=start_frame;i<=end_frame;++i){
    int_t num_digits_total = 0;
    int_t decrement_total = num_images;
    int_t num_digits_frame = 0;
    int_t decrement_subset = i;
    while (decrement_total){decrement_total /= 10; num_digits_total++;}
    if(i==0) num_digits_frame = 1;
    else
      while (decrement_subset){decrement_subset /= 10; num_digits_frame++;}
    int_t num_zeros = num_digits_total - num_digits_frame;
    // determine the file name for this subset
    std::stringstream fName;
    fName << prefix << "_";
    for(int_t j=0;j<num_zeros;++j)
      fName << "0";
    fName << i << ".tif";
     Teuchos::RCP<DICe::Image> image = cine_reader->get_frame(i);
    if(rotation!=0){
      if(rotation==90){
        image = image->apply_rotation(NINTY_DEGREES);
      }else if(rotation==180){
        image = image->apply_rotation(ONE_HUNDRED_EIGHTY_DEGREES);
      }else if(rotation==270){
        image = image->apply_rotation(TWO_HUNDRED_SEVENTY_DEGREES);
      }
      else{
        if(i==start_frame)
          std::cout << "WARNING: user requested invalid rotation: " << rotation << " must be 90, 180 or 270. Skipping image rotation" << std::endl;
      }
    }
    image->write(fName.str());
  }

  DICe::finalize();

  return 0;
}

