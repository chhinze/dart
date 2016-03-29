/*
 * Copyright (c) 2016, Georgia Tech Research Corporation
 * All rights reserved.
 *
 * Author(s): Michael X. Grey <mxgrey@gatech.edu>
 *
 * Georgia Tech Graphics Lab and Humanoid Robotics Lab
 *
 * Directed by Prof. C. Karen Liu and Prof. Mike Stilman
 * <karenliu@cc.gatech.edu> <mstilman@cc.gatech.edu>
 *
 * This file is provided under the following "BSD-style" License:
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 *   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DART_COMMON_DETAIL_EMBEDDEDASPECT_H_
#define DART_COMMON_DETAIL_EMBEDDEDASPECT_H_

#include "dart/common/Aspect.h"
#include "dart/common/StlHelpers.h"

namespace dart {
namespace common {
namespace detail {

//==============================================================================
template <class AspectT, typename StateT>
void DefaultSetEmbeddedState(AspectT* aspect, const StateT& state)
{
  aspect->getComposite()->setAspectState(state);
}

//==============================================================================
template <class AspectT, typename StateT>
const StateT& DefaultGetEmbeddedState(const AspectT* aspect)
{
  return aspect->getComposite()->getAspectState();
}

//==============================================================================
template <class AspectT, typename PropertiesT>
void DefaultSetEmbeddedProperties(AspectT* aspect, const PropertiesT& properties)
{
  aspect->getComposite()->setAspectProperties(properties);
}

//==============================================================================
template <class AspectT, typename PropertiesT>
const PropertiesT& DefaultGetEmbeddedProperties(const AspectT* aspect)
{
  return aspect->getComposite()->getAspectProperties();
}

//==============================================================================
template <class BaseT, class DerivedT, typename StateT,
          void (*setEmbeddedState)(DerivedT*, const StateT&),
          const StateT& (*getEmbeddedState)(const DerivedT*)>
class EmbeddedStateAspect : public BaseT
{
public:

  using Base = BaseT;
  using Derived = DerivedT;
  using State = StateT;
  constexpr static void (*SetEmbeddedState)(Derived*, const State&) = setEmbeddedState;
  constexpr static const State& (*GetEmbeddedState)(const Derived*) = getEmbeddedState;

  EmbeddedStateAspect() = delete;
  EmbeddedStateAspect(const EmbeddedStateAspect&) = delete;

  /// Construct using a State instance
  EmbeddedStateAspect(Composite* comp, const State& state = State())
    : BaseT(comp),
      mTemporaryState(make_unique(state))
  {
    // Do nothing
  }

  /// Construct this Aspect and pass args into the constructor of the Base class
  template <typename... BaseArgs>
  EmbeddedStateAspect(
      Composite* comp, const State& state, BaseArgs&&... args)
    : Base(comp, std::forward<BaseArgs>(args)...),
      mTemporaryState(make_unique(state))
  {
    // Do nothing
  }

  // Documentation inherited
  void setAspectState(const Aspect::State& state) override final
  {
    setState(static_cast<const State&>(state));
  }

  // Documentation inherited
  void setState(const State& state)
  {
    if(this->getComposite())
    {
      SetEmbeddedState(this, state);
      return;
    }

    // If the correct type of Composite is not available, we store this on the
    // heap until this Aspect is moved.
    mTemporaryState = make_unique(state);
  }

  // Documentation inherited
  const Aspect::State* getAspectState() const override final
  {
    return &getState();
  }

  // Documentation inherited
  const State& getState() const
  {
    if(this->getComposite())
    {
      return GetEmbeddedState(this);
    }

    if(!mTemporaryState)
      mTemporaryState = make_unique<State>();

    return *mTemporaryState;
  }

protected:

  /// Pass the temporary State of this Aspect into the new Composite
  void setComposite(Composite* newComposite) override
  {
    Base::setComposite(newComposite);
    if(mTemporaryState)
      SetEmbeddedState(this, *mTemporaryState);
    else
      SetEmbeddedState(this, State());

    mTemporaryState = nullptr;
  }

  /// Save the embedded State of this Composite before we remove the Aspect
  void loseComposite(Composite* oldComposite) override
  {
    mTemporaryState = make_unique(GetEmbeddedState(this));
    Base::loseComposite(oldComposite);
  }

  /// During transitions between Composite objects, this will hold the State of
  /// the Aspect. Once the Aspect has been moved into a new Composite, this
  /// State will be pushed into the Composite and cleared.
  std::unique_ptr<State> mTemporaryState;

};

//==============================================================================
template <class BaseT, class DerivedT, typename PropertiesT,
          void (*setEmbeddedProperties)(DerivedT*, const PropertiesT&) =
              &DefaultSetEmbeddedProperties<DerivedT, PropertiesT>,
          const PropertiesT& (*getEmbeddedProperties)(const DerivedT*) =
              &DefaultGetEmbeddedProperties<DerivedT, PropertiesT> >
class EmbeddedPropertiesAspect : public BaseT
{
public:

  using Base = BaseT;
  using Derived = DerivedT;
  using Properties = PropertiesT;
  constexpr static void (*SetEmbeddedProperties)(Derived*, const Properties&) = setEmbeddedProperties;
  constexpr static const Properties& (*GetEmbeddedProperties)(const Derived*) = getEmbeddedProperties;

  EmbeddedPropertiesAspect() = delete;
  EmbeddedPropertiesAspect(const EmbeddedPropertiesAspect&) = delete;

  /// Construct using a Properties instance
  EmbeddedPropertiesAspect(
      Composite* comp, const Properties& properties = Properties())
    : BaseT(comp),
      mTemporaryProperties(make_unique(properties))
  {
    // Do nothing
  }

  /// Construct this Aspect and pass args into the constructor of the Base class
  template <typename... BaseArgs>
  EmbeddedPropertiesAspect(
      Composite* comp, const Properties& properties, BaseArgs&&... args)
    : Base(comp, std::forward<BaseArgs>(args)...),
      mTemporaryProperties(make_unique(properties))
  {
    // Do nothing
  }

  // Documentation inherited
  void setAspectProperties(const Aspect::Properties& properties) override final
  {
    setProperties(static_cast<const Properties&>(properties));
  }

  // Documentation inherited
  void setProperties(const Properties& properties)
  {
    if(this->getComposite())
    {
      SetEmbeddedProperties(this, properties);
      return;
    }

    // If the correct type of Composite is not available, we store this on the
    // heap until this Aspect is moved.
    mTemporaryProperties = make_unique(properties);
  }

  // Documentation inherited
  const Aspect::Properties* getAspectProperties() const override final
  {
    return &getProperties();
  }

  // Documentation inherited
  const Properties& getProperties() const
  {
    if(this->getComposite())
    {
      return GetEmbeddedProperties(this);
    }

    if(!mTemporaryProperties)
      mTemporaryProperties = make_unique<Properties>();

    return *mTemporaryProperties;
  }

protected:

  /// Pass the temporary Properties of this Aspect into the new Composite
  void setComposite(Composite* newComposite) override
  {
    Base::setComposite(newComposite);
    if(mTemporaryProperties)
      SetEmbeddedProperties(this, *mTemporaryProperties);
    else
      SetEmbeddedProperties(this, Properties());

    mTemporaryProperties = nullptr;
  }

  /// Save the embedded Properties of this Composite before we remove the Aspect
  void loseComposite(Composite* oldComposite)
  {
    mTemporaryProperties = make_unique(GetEmbeddedProperties(this));
    Base::loseComposite(oldComposite);
  }

  /// During transitions between Composite objects, this will hold the Properties
  /// of the Aspect. Once the Aspect has been moved into a new Composite, these
  /// Properties will be pushed into the Composite and cleared.
  std::unique_ptr<Properties> mTemporaryProperties;

};


//==============================================================================
//
// These namespace-level definitions are required to enable ODR-use of static
// constexpr member variables.
//
// See this StackOverflow answer: http://stackoverflow.com/a/14396189/111426
//
template <class BaseT, class DerivedT, typename StateT,
          void (*setEmbeddedState)(DerivedT*, const StateT&),
          const StateT& (*getEmbeddedState)(const DerivedT*)>
constexpr void (*EmbeddedStateAspect<
    BaseT, DerivedT, StateT, setEmbeddedState,
    getEmbeddedState>::SetEmbeddedState)(DerivedT*, const StateT&);

//==============================================================================
template <class BaseT, class DerivedT, typename StateT,
          void (*setEmbeddedState)(DerivedT*, const StateT&),
          const StateT& (*getEmbeddedState)(const DerivedT*)>
constexpr const StateT& (*EmbeddedStateAspect<
    BaseT, DerivedT, StateT, setEmbeddedState,
    getEmbeddedState>::GetEmbeddedState)(const DerivedT*);

//==============================================================================
template <class BaseT, class DerivedT, typename PropertiesT,
          void (*setEmbeddedProperties)(DerivedT*, const PropertiesT&),
          const PropertiesT& (*getEmbeddedProperties)(const DerivedT*)>
constexpr void (*EmbeddedPropertiesAspect<
    BaseT, DerivedT, PropertiesT, setEmbeddedProperties,
    getEmbeddedProperties>::SetEmbeddedProperties)(DerivedT*, const PropertiesT&);

//==============================================================================
template <class BaseT, class DerivedT, typename PropertiesT,
          void (*setEmbeddedProperties)(DerivedT*, const PropertiesT&),
          const PropertiesT& (*getEmbeddedProperties)(const DerivedT*)>
constexpr const PropertiesT& (*EmbeddedPropertiesAspect<
    BaseT, DerivedT, PropertiesT, setEmbeddedProperties,
    getEmbeddedProperties>::GetEmbeddedProperties)(const DerivedT*);

} // namespace detail
} // namespace common
} // namespace dart

#endif // DART_COMMON_DETAIL_EMBEDDEDASPECT_H_