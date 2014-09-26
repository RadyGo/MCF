// MCF Build
// Copyleft 2014, LH_Mouse. All wrongs reserved.

#ifndef MCFBUILD_MCFBUILD_HPP_
#define MCFBUILD_MCFBUILD_HPP_

#include "../MCF/StdMCF.hpp"
#include "../MCF/Core/String.hpp"
#include "../MCF/Core/Exception.hpp"
#include "../MCF/Utilities/Utilities.hpp"

#include "Localization.hpp"

#define FORMAT_THROW(code, msg)		MCF_THROW(code, ::MCFBuild::FormatString(msg))

#endif
