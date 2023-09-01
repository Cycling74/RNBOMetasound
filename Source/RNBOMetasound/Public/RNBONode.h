#pragma once

#include "MetasoundFacade.h"

namespace RNBOMetasound {
  template <class Op>
		class FGenericNode : public Metasound::FNodeFacade
	{
		public:
			/**
			 * Constructor used by the Metasound Frontend.
			 */
			FGenericNode(const Metasound::FNodeInitData& InitData)
				: Metasound::FNodeFacade(InitData.InstanceName, InitData.InstanceID, Metasound::TFacadeOperatorClass<Op>())
			{
			}
  };
}
