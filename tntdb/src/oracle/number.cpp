/* 
 * Copyright (C) 2007 Tommi Maekitalo, Mark Wright
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <tntdb/oracle/number.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string.h>
#include <math.h>

namespace tntdb
{
  namespace oracle
  {
    Number::Number()
    {
      memset(vnum, 0, OCI_NUMBER_SIZE);
    }
    
    Number::Number(const Decimal &decimal)
    {
      memset(vnum, 0, OCI_NUMBER_SIZE);
      if (decimal.isInfinity())
      {
        if (decimal.isInfinity(true))
        {
          vnum[0] = 0x01;
          vnum[1] = 0x00;
        }
        else
        {
          vnum[0] = 0x00;
          vnum[1] = 0xff;
          vnum[2] = 0x65;
        }
      }
      else
      {
        if (decimal.isZero())
        {
          vnum[0] = 0x01;
          vnum[1] = 0x80;
        }
        else
        {
          const int rightNumberLength = OCI_NUMBER_SIZE;
          unsigned char rightNumber[rightNumberLength];
          tntdb::Decimal::ExponentType noBaseOneHundredDigits = 1;
          tntdb::Decimal::MantissaType x = decimal.getMantissa();
          tntdb::Decimal::ExponentType baseOneHundredExponent = decimal.getExponent() * 2;
          if (decimal.isPositive())
          {
            tntdb::Decimal::MantissaType digit = x % tntdb::Decimal::MantissaType(100) + tntdb::Decimal::MantissaType(1);
            rightNumber[rightNumberLength - 1] = (unsigned char)digit;
            while ((x /= tntdb::Decimal::MantissaType(100)) != 0)
            {
              digit = x % tntdb::Decimal::MantissaType(100) + 1;
              rightNumber[rightNumberLength - (1 + noBaseOneHundredDigits)] = (unsigned char)digit;
              noBaseOneHundredDigits += 1;
            }
            vnum[0] = (unsigned char)(noBaseOneHundredDigits + 1);
            tntdb::Decimal::ExponentType exponent_base = 64 + baseOneHundredExponent + noBaseOneHundredDigits;
            vnum[1] = (unsigned char)(0x80 + exponent_base);
            memcpy(&vnum[2], &rightNumber[rightNumberLength - noBaseOneHundredDigits], noBaseOneHundredDigits);
          }
          else
          {
            tntdb::Decimal::MantissaType digit = tntdb::Decimal::MantissaType(101) - (x % tntdb::Decimal::MantissaType(100));
            rightNumber[rightNumberLength - 1] = (unsigned char)digit;
            while ((x /= tntdb::Decimal::MantissaType(100)) != 0)
            {
              digit = tntdb::Decimal::MantissaType(101) - (x % tntdb::Decimal::MantissaType(100));
              rightNumber[rightNumberLength - (1 + noBaseOneHundredDigits)] = (unsigned char)digit;
              noBaseOneHundredDigits += 1;
            } 
            memcpy(&vnum[2], &rightNumber[rightNumberLength - noBaseOneHundredDigits], noBaseOneHundredDigits);
            tntdb::Decimal::ExponentType exponent_base = (63 - noBaseOneHundredDigits) + baseOneHundredExponent;
            vnum[1] = (unsigned char)exponent_base;
            if (noBaseOneHundredDigits < 20)
            {
              vnum[0] = (unsigned char)(noBaseOneHundredDigits + 2);
              vnum[2 + noBaseOneHundredDigits] = (unsigned char)102;
            }
            else
            {
              vnum[0] = (unsigned char)(noBaseOneHundredDigits + 1);
            }
          }
        }
      }
    }
    
    Decimal Number::getDecimal() const
    {
      // Negative infinity is represented by 0x00, and positive infinity is represented by the two bytes 0xFF65
      bool positive = vnum[1] & 0x80;
      bool infinity = false;
      tntdb::Decimal::ExponentType baseOneHundredExponent = vnum[1] & 0x7f;
      tntdb::Decimal::MantissaType mantissa = 0;
      int length = vnum[0];
      if (length > OCI_NUMBER_SIZE)
        length = OCI_NUMBER_SIZE;
      if (length == 1)
      {
        if (vnum[1] == 0)
        {
          // Negative infinity is represented by the single byte 0x00.
          infinity = true;
        }
        else
        {
          // Zero is represented by the single byte 0x80.
          mantissa = 0;
        }
      }
      else
      {
        if (positive)
        {
          mantissa = tntdb::Decimal::MantissaType(vnum[2] - 1);
          if ((length == 2) && (baseOneHundredExponent == 0x7f) && (mantissa == 0x64))
          {
            // Positive infinity is represented by the two bytes 0xff65.
            mantissa = ~tntdb::Decimal::MantissaType(0);
            infinity = true;
          }
          else
          {
            infinity = false;
            baseOneHundredExponent -= (65 + length - 2);
            for (int i = 3; i <= length; ++i)
            {
              mantissa = (mantissa * tntdb::Decimal::MantissaType(100)) + tntdb::Decimal::MantissaType(vnum[i] - 1);
            }
          }
        }
        else
        {
          baseOneHundredExponent =  -(baseOneHundredExponent - 64) - (length - 1);
          mantissa = tntdb::Decimal::MantissaType(101 - vnum[2]);
          if (length < OCI_NUMBER_SIZE)
            --length;  // ignore terminator byte value of 102 decimal for negative numbers with less than 20 base 100 mantissa digits.
          for (int i = 3; i <= length; ++i)
          {
            mantissa = (mantissa  * tntdb::Decimal::MantissaType(100)) + tntdb::Decimal::MantissaType(101 - vnum[i]);
          }
        }
      }
      tntdb::Decimal::FlagsType flags = 0;
      if (positive)
        flags |= tntdb::Decimal::positive;
      if (infinity)
        flags |= tntdb::Decimal::infinity;
      Decimal decimal(mantissa, baseOneHundredExponent * 2, flags, tntdb::Decimal::infinityTilde);
      return decimal;
    }
  };
};
