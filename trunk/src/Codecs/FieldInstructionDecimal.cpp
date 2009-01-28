// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#include "FieldInstructionDecimal.h"
#include <Codecs/DataSource.h>
#include <Codecs/Decoder.h>
#include <Codecs/FieldInstructionMantissa.h>
#include <Codecs/FieldInstructionExponent.h>
#include <Messages/Message.h>
#include <Messages/FieldSet.h>
#include <Messages/FieldDecimal.h>
#include <Common/Decimal.h>

#include <Common/Profiler.h>

using namespace ::QuickFAST;
using namespace ::QuickFAST::Codecs;

FieldInstructionDecimal::FieldInstructionDecimal(
      const std::string & name,
      const std::string & fieldNamespace)
  : FieldInstruction(name, fieldNamespace)
  , typedExponent_(0)
  , typedMantissa_(0)
  , typedValue_(0,0)
  , mantissaInstruction_(new FieldInstructionMantissa(name + "|decimal_mantissa", fieldNamespace))
  , exponentInstruction_(new FieldInstructionExponent(name + "|decimal_exponent", fieldNamespace))
{
}

FieldInstructionDecimal::FieldInstructionDecimal()
  : typedExponent_(0)
  , typedMantissa_(0)
  , typedValue_(0,0)
  , mantissaInstruction_(new FieldInstructionMantissa(identity_.getLocalName() + "|decimal_mantissa", identity_.getNamespace()))
  , exponentInstruction_(new FieldInstructionExponent(identity_.getLocalName() + "|decimal_exponent", identity_.getNamespace()))
{
}

FieldInstructionDecimal::~FieldInstructionDecimal()
{
}

void
FieldInstructionDecimal::setPresence(bool mandatory)
{
  FieldInstruction::setPresence(mandatory);
  if(!mandatory)
  {
    exponentInstruction_->setPresence(mandatory);
  }
}


bool
FieldInstructionDecimal::decodeNop(
  Codecs::DataSource & source,
  Codecs::PresenceMap & pmap,
  Codecs::Decoder & decoder,
  Messages::FieldSet & fieldSet) const
{
  PROFILE_POINT("decimal::decodeNop");

  ///@todo: this is an expensive way to get the exponent and mantissa values
  /// we need to refactor FieldInstructionInteger to provide a separate method
  /// to retrieve the values (or lack thereof) without creating fields.
  /// However -- this works for now.
  Messages::FieldSet mxSet(2);
  {PROFILE_POINT("decimal::DecodeExponent");
  if(!exponentInstruction_->decode(source, pmap, decoder, mxSet))
  {
    return false;
  }
  } // PROFILE
  if(mxSet.size() == 0)
  {
    // null field
    return true;
  }

  {PROFILE_POINT("decimal::DecodeMantissa");
  mantissaInstruction_->decode(source, pmap, decoder, mxSet);
  }
  NESTED_PROFILE_POINT(1, "decimal::RetrieveValues");
  Messages::FieldSet::const_iterator it = mxSet.begin();
  exponent_t exponent = exponent_t(it->getField()->toInt32());
  mantissa_t mantissa = typedMantissa_;
  ++it;
  if(it != mxSet.end())
  {
    mantissa = mantissa_t(it->getField()->toInt32());
  }
  NESTED_PROFILE_PAUSE(1);
  NESTED_PROFILE_POINT(2, "decimal::UseValues");

  Decimal value(mantissa, exponent, false);
  Messages::FieldCPtr field(Messages::FieldDecimal::create(value));
  fieldSet.addField(identity_, field);
  return true;
}

bool
FieldInstructionDecimal::decodeConstant(
  Codecs::DataSource & source,
  Codecs::PresenceMap & pmap,
  Codecs::Decoder & decoder,
  Messages::FieldSet & fieldSet) const
{
  PROFILE_POINT("decimal::decodeConstant");
  if(isMandatory() || pmap.checkNextField())
  {
    Messages::FieldCPtr newField(Messages::FieldDecimal::create(typedValue_));
    fieldSet.addField(
      identity_,
      newField);
  }
  return true;
}

bool
FieldInstructionDecimal::decodeDefault(
  Codecs::DataSource & source,
  Codecs::PresenceMap & pmap,
  Codecs::Decoder & decoder,
  Messages::FieldSet & fieldSet) const
{
  PROFILE_POINT("decimal::decodeDefault");
  if(pmap.checkNextField())
  {
    exponent_t exponent = 0;
    decodeSignedInteger(source, decoder, exponent);
    if(!isMandatory())
    {
      if(checkNullInteger(exponent))
      {
        return true;
      }
    }
    mantissa_t mantissa;
    decodeSignedInteger(source, decoder, mantissa);
    Decimal value(mantissa, exponent);
    Messages::FieldCPtr newField(Messages::FieldDecimal::create(value));
    fieldSet.addField(
      identity_,
      newField);
  }
  else // field not in stream
  {
    if(isMandatory())
    {
      Messages::FieldCPtr newField(Messages::FieldDecimal::create(typedValue_));
      fieldSet.addField(
        identity_,
        newField);
    }
    else if(isMandatory())
    {
      throw EncodingError("[ERR D5]Mandatory default operator with no value.");
    }
  }
  return true;
}

bool
FieldInstructionDecimal::decodeCopy(
  Codecs::DataSource & source,
  Codecs::PresenceMap & pmap,
  Codecs::Decoder & decoder,
  Messages::FieldSet & fieldSet) const
{
  PROFILE_POINT("decimal::decodeCopy");
  DictionaryPtr dictionary;
  getDictionary(decoder, dictionary);

  exponent_t exponent = 0;
  mantissa_t mantissa = 0;
  if(pmap.checkNextField())
  {
    decodeSignedInteger(source, decoder, exponent);
    if(isMandatory())
    {
      decodeSignedInteger(source, decoder, mantissa);
      Decimal value(mantissa, exponent, false);
      Messages::FieldCPtr newField(Messages::FieldDecimal::create(value));
      fieldSet.addField(
        identity_,
        newField);
      dictionary->add(getKey(), newField);
    }
    else
    {
      // not mandatory means it's nullable
      if(checkNullInteger(exponent))
      {
        Messages::FieldCPtr newField(Messages::FieldDecimal::createNull());
        dictionary->add(getKey(), newField);
      }
      else
      {
        decodeSignedInteger(source, decoder, mantissa);
        Decimal value(mantissa, exponent, false);
        Messages::FieldCPtr newField(Messages::FieldDecimal::create(value));
        fieldSet.addField(
          identity_,
          newField);
        dictionary->add(getKey(), newField);
      }
    }

  }
  else // pmap says not present, use copy
  {
    Messages::FieldCPtr previousField;
    if(dictionary->find(getKey(), previousField))
    {
      //Decimal previous;
      if(previousField->isDefined())
      {
        if(previousField->isType(Messages::Field::DECIMAL))
        {
          fieldSet.addField(
            identity_,
            previousField);
        }
        else
        {
          throw TemplateDefinitionError("[ERR D4] Previous value type mismatch.");
        }
      }
      else // field present but not defined
      {
        if(isMandatory())
        {
          throw TemplateDefinitionError("[ERR D6] Mandatory field is missing.");
        }
      }
    }
    else
    {
      // value not found in dictionary
      // not a problem..  use initial value if it's available
      if(fieldOp_->hasValue())
      {
        Messages::FieldCPtr newField(Messages::FieldDecimal::create(typedValue_));
        fieldSet.addField(
          identity_,
          newField);
        dictionary->add(getKey(), newField);
      }
      else
      {
        if(isMandatory())
        {
          throw EncodingError("[ERR D5] Copy operator missing mandatory Decimal field/no initial value");
        }
      }
    }
  }
  return true;
}

bool
FieldInstructionDecimal::decodeDelta(
  Codecs::DataSource & source,
  Codecs::PresenceMap & pmap,
  Codecs::Decoder & decoder,
  Messages::FieldSet & fieldSet) const
{
  PROFILE_POINT("decimal::decodeDelta");
  int64 exponentDelta;
  decodeSignedInteger(source, decoder, exponentDelta, true);
  if(!isMandatory())
  {
    if(checkNullInteger(exponentDelta))
    {
      // nothing in Message; no change to saved value
      return true;
    }
  }
  int64 mantissaDelta;
  decodeSignedInteger(source, decoder, mantissaDelta, true);

  DictionaryPtr dictionary;
  getDictionary(decoder, dictionary);

  Decimal value(typedValue_);
  Messages::FieldCPtr previousField;
  if(dictionary->find(getKey(), previousField))
  {
    if(previousField->isType(Messages::Field::DECIMAL))
    {
      value = previousField->toDecimal();
    }
    else
    {
      throw TemplateDefinitionError("[ERR D4] Previous value type mismatch.");
    }
  }
  value.setExponent(exponent_t(value.getExponent() + exponentDelta));
  value.setMantissa(mantissa_t(value.getMantissa() + mantissaDelta));
  Messages::FieldCPtr newField(Messages::FieldDecimal::create(value));
  fieldSet.addField(
    identity_,
    newField);
  dictionary->add(getKey(), newField);
  return true;
}
void
FieldInstructionDecimal::encodeNullableDecimal(
  Codecs::DataDestination & destination,
  WorkingBuffer & buffer,
  exponent_t exponent,
  mantissa_t mantissa) const
{
  if(exponent > 0)
  {
    exponent += 1;
  }
  encodeDecimal(destination, buffer, exponent, mantissa);
}

void
FieldInstructionDecimal::encodeDecimal(
  Codecs::DataDestination & destination,
  WorkingBuffer & buffer,
  exponent_t exponent,
  mantissa_t mantissa) const
{
  encodeSignedInteger(destination, buffer, exponent);
  encodeSignedInteger(destination, buffer, mantissa);
}

void
FieldInstructionDecimal::encodeNop(
  Codecs::DataDestination & destination,
  Codecs::PresenceMap & pmap,
  Codecs::Encoder & encoder,
  const Messages::FieldSet & fieldSet) const
{
  // get the value from the application data
  Messages::FieldCPtr field;
  if(fieldSet.getField(identity_.name(), field))
  {
    Decimal value = field->toDecimal();
    exponent_t exponent = value.getExponent();
    mantissa_t mantissa = value.getMantissa();

    // @todo: replace this with something more efficient
    Messages::FieldSet fieldSet(2);
    Messages::FieldCPtr exponentField(Messages::FieldInt32::create(exponent));
    fieldSet.addField(this->exponentInstruction_->getIdentity(), exponentField);

    Messages::FieldCPtr mantissaField(Messages::FieldInt32::create(mantissa));
    fieldSet.addField(mantissaInstruction_->getIdentity(), mantissaField);

    exponentInstruction_->encode(
      destination,
      pmap,
      encoder,
      fieldSet);
    mantissaInstruction_->encode(
      destination,
      pmap,
      encoder,
      fieldSet);
  }
  else // not defined in fieldset
  {
    if(isMandatory())
    {
      throw EncodingError("Missing mandatory field.");
    }
    Messages::FieldSet fieldSet(2);
    exponentInstruction_->encode(
      destination,
      pmap,
      encoder,
      fieldSet);
  }
}

void
FieldInstructionDecimal::encodeConstant(
  Codecs::DataDestination & destination,
  Codecs::PresenceMap & pmap,
  Codecs::Encoder & encoder,
  const Messages::FieldSet & fieldSet) const
{
  // get the value from the application data
  Messages::FieldCPtr field;
  if(fieldSet.getField(identity_.name(), field))
  {
    Decimal value = field->toDecimal();
    if(value != typedValue_)
    {
      throw EncodingError("Constant value does not match application data.");
    }

    if(!isMandatory())
    {
      pmap.setNextField(true);
    }
  }
  else // not defined in fieldset
  {
    if(isMandatory())
    {
      throw EncodingError("Missing mandatory field.");
    }
    pmap.setNextField(false);
  }
}

void
FieldInstructionDecimal::encodeDefault(
  Codecs::DataDestination & destination,
  Codecs::PresenceMap & pmap,
  Codecs::Encoder & encoder,
  const Messages::FieldSet & fieldSet) const
{
  // get the value from the application data
  Messages::FieldCPtr field;
  if(fieldSet.getField(identity_.name(), field))
  {
    Decimal value = field->toDecimal();
    if(value == typedValue_)
    {
      pmap.setNextField(false); // not in stream. use default
    }
    else
    {
      pmap.setNextField(true); // != default.  Send value

      if(!isMandatory())
      {
        encodeNullableDecimal(destination, encoder.getWorkingBuffer(), value.getExponent(), value.getMantissa());
      }
      else
      {
        encodeDecimal(destination, encoder.getWorkingBuffer(), value.getExponent(), value.getMantissa());
      }
    }
  }
  else // not defined in fieldset
  {
    if(isMandatory())
    {
      throw EncodingError("Missing mandatory field.");
    }
    if(fieldOp_->hasValue())
    {
      pmap.setNextField(true);
      destination.putByte(nullDecimal);
    }
    else
    {
      pmap.setNextField(false);
    }
  }
}


void
FieldInstructionDecimal::encodeCopy(
  Codecs::DataDestination & destination,
  Codecs::PresenceMap & pmap,
  Codecs::Encoder & encoder,
  const Messages::FieldSet & fieldSet) const
{
  // declare a couple of variables...
  bool previousIsKnown = false;
  bool previousNotNull = false;
  Decimal previousValue(0,0);

  // ... then initialize them from the dictionary
  DictionaryPtr dictionary;
  getDictionary(encoder, dictionary);
  Messages::FieldCPtr previousField;
  if(dictionary->find(getKey(), previousField))
  {
    if(!previousField->isType(Messages::Field::DECIMAL))
    {
      throw TemplateDefinitionError("[ERR D4] Previous value type mismatch.");
    }
    previousIsKnown = true;
    previousNotNull = previousField->isDefined();
    if(previousNotNull)
    {
      previousValue = previousField->toDecimal();
    }
  }

  // get the value from the application data
  Messages::FieldCPtr field;
  if(fieldSet.getField(identity_.name(), field))
  {
    Decimal value = field->toDecimal();

    if(previousIsKnown && previousValue == value)
    {
      pmap.setNextField(false); // not in stream, use copy
    }
    else
    {
      if(!isMandatory())
      {
        pmap.setNextField(true);// value in stream
        encodeNullableDecimal(destination, encoder.getWorkingBuffer(), value.getExponent(), value.getMantissa());
        field = Messages::FieldDecimal::create(value);
        dictionary->add(getKey(), field);
      }
    }
  }
  else // not defined in fieldset
  {
    if(isMandatory())
    {
      throw EncodingError("Missing mandatory field.");
    }
    if((previousIsKnown && previousNotNull)
      || !previousIsKnown)
    {
      pmap.setNextField(true);// value in stream
      destination.putByte(nullDecimal);
      field = Messages::FieldDecimal::createNull();
      dictionary->add(getKey(), field);
    }
    else
    {
      pmap.setNextField(false);
    }
  }
}


void
FieldInstructionDecimal::encodeDelta(
  Codecs::DataDestination & destination,
  Codecs::PresenceMap & pmap,
  Codecs::Encoder & encoder,
  const Messages::FieldSet & fieldSet) const
{

  // declare a couple of variables...
  bool previousIsKnown = false;
  bool previousNotNull = false;
  Decimal previousValue(typedValue_);

  // ... then initialize them from the dictionary
  DictionaryPtr dictionary;
  getDictionary(encoder, dictionary);
  Messages::FieldCPtr previousField;
  if(dictionary->find(getKey(), previousField))
  {
    if(!previousField->isType(Messages::Field::DECIMAL))
    {
      throw TemplateDefinitionError("[ERR D4] Previous value type mismatch.");
    }
    previousIsKnown = true;
    previousNotNull = previousField->isDefined();
    if(previousNotNull)
    {
      previousValue = previousField->toDecimal();
    }
  }

  // get the value from the application data
  Messages::FieldCPtr field;
  if(fieldSet.getField(identity_.name(), field))
  {
    Decimal value = field->toDecimal();

    int64 exponentDelta = int64(value.getExponent()) - int64(previousValue.getExponent());
    if(!isMandatory())
    {
      if(exponentDelta >= 0)
      {
        exponentDelta += 1;
      }
    }
    encodeSignedInteger(destination, encoder.getWorkingBuffer(), exponentDelta);

    int64 mantissaDelta = int64(value.getMantissa()) - int64(previousValue.getMantissa());
    encodeSignedInteger(destination, encoder.getWorkingBuffer(), mantissaDelta);

    if(!previousIsKnown  || value != previousValue)
    {
      field = Messages::FieldDecimal::create(value);
      dictionary->add(getKey(), field);
    }

  }
  else // not defined in fieldset
  {
    if(isMandatory())
    {
      throw EncodingError("Missing mandatory field.");
    }
    destination.putByte(nullInteger);
  }
}

void
FieldInstructionDecimal::interpretValue(const std::string & value)
{
  typedValue_.parse(value);
  typedMantissa_ = typedValue_.getMantissa();
  typedExponent_ = typedValue_.getExponent();
}

size_t
FieldInstructionDecimal::maxPresenceMapBits()const
{
  return 2;
}