// Filters.hpp
#pragma once
#ifndef SWIG
#define GAMMA_H_INC_ALL 1
#include <Gamma/Gamma.h>
#endif
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <atomic>
#include <algorithm>

#include "Engine.hpp"
#include "Parameters.hpp"

#include "Biquad.hpp"
#include "OnePole.hpp"
#include "BlockDC.hpp"
#include "BlockNyq.hpp"
#include "AllPass1.hpp"
#include "AllPass2.hpp"
#include "Notch.hpp"
#include "Reson.hpp"
#include "Hilbert.hpp"
#include "Integrator.hpp"
#include "Differencer.hpp"
#include "MovingAvg.hpp"