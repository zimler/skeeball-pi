// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Copyright (C) 2013 Henner Zeller <h.zeller@acm.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://gnu.org/licenses/gpl-2.0.txt>

#include "led-matrix.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define SHOW_REFRESH_RATE 0

#if SHOW_REFRESH_RATE
# include <stdio.h>
# include <sys/time.h>
#endif

#include "gpio.h"
#include "thread.h"
#include "framebuffer-internal.h"

namespace rgb_matrix {

// Pump pixels to screen. Needs to be high priority real-time because jitter
class RGBMatrix::UpdateThread : public Thread {
public:
  UpdateThread(RGBMatrix *matrix) : running_(true), matrix_(matrix) {}

  void Stop() {
    MutexLock l(&mutex_);
    running_ = false;
  }

  virtual void Run() {
    while (running()) {
#if SHOW_REFRESH_RATE
      struct timeval start, end;
      gettimeofday(&start, NULL);
#endif
      matrix_->UpdateScreen();
#if SHOW_REFRESH_RATE
      gettimeofday(&end, NULL);
      int64_t usec = ((uint64_t)end.tv_sec * 1000000 + end.tv_usec)
        - ((int64_t)start.tv_sec * 1000000 + start.tv_usec);
      printf("\b\b\b\b\b\b\b\b%6.1fHz", 1e6 / usec);
#endif
    }
  }

private:
  int cols,rows,height;
  inline bool running() {
    MutexLock l(&mutex_);
    return running_;
  }

  Mutex mutex_;
  bool running_;
  RGBMatrix *const matrix_;
};

RGBMatrix::RGBMatrix(GPIO *io, int height, int rows, int cols)
  : frame_(new Framebuffer(height, 32 * cols * rows)),
    io_(NULL), updater_(NULL) {
  Clear();
  SetGPIO(io);
  this.cols = cols;
  this.rows = rows;
  this.height = height;
}

RGBMatrix::~RGBMatrix() {
  updater_->Stop();
  updater_->WaitStopped();
  delete updater_;

  frame_->Clear();
  frame_->DumpToMatrix(io_);
  delete frame_;
}

void RGBMatrix::SetGPIO(GPIO *io) {
  if (io == NULL) return;  // nothing to set.
  if (io_ != NULL) return;  // already set.
  io_ = io;
  Framebuffer::InitGPIO(io_);
  updater_ = new UpdateThread(this);
  updater_->Start(99);  // Whatever we get :)
}

bool RGBMatrix::SetPWMBits(uint8_t value) { return frame_->SetPWMBits(value); }
uint8_t RGBMatrix::pwmbits() { return frame_->pwmbits(); }

// Map brightness of output linearly to input with CIE1931 profile.
void RGBMatrix::set_luminance_correct(bool on) {
  frame_->set_luminance_correct(on);
}
bool RGBMatrix::luminance_correct() const { return frame_->luminance_correct(); }
void RGBMatrix::UpdateScreen() { frame_->DumpToMatrix(io_); }

// -- Implementation of RGBMatrix Canvas: delegation to ContentBuffer
int RGBMatrix::width() const { return 32*this.cols }
int RGBMatrix::height() const { return this.height*this.rows }
void RGBMatrix::SetPixel(int x, int y,
                         uint8_t red, uint8_t green, uint8_t blue) {
  if (x < 0 || x >= width() || y < 0 || y >= height()) return;
    // We have up to column 64 one direction, then folding around. Lets map
    if (y >= this.height) {
      int r = (int)(y/this.height);
      x =  x-(width()*r);
      y = y%this.height;
    }
  frame_->SetPixel(x, y, red, green, blue);
}
void RGBMatrix::Clear() { return frame_->Clear(); }
void RGBMatrix::Fill(uint8_t red, uint8_t green, uint8_t blue) {
  frame_->Fill(red, green, blue);
}
}  // end namespace rgb_matrix
