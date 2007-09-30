/* 
 * Copyright (C) 2007 Tommi Maekitalo
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

#ifndef TNTDB_ORACLE_SQLSTATEMENT_H
#define TNTDB_ORACLE_SQLSTATEMENT_H

#include <tntdb/iface/istatement.h>
#include <oci.h>
#include <cxxtools/dynbuffer.h>
#include <string>
#include <string.h>
#include <map>
#include <tntdb/oracle/datetime.h>
#include <tntdb/oracle/number.h>

namespace tntdb
{
  namespace oracle
  {
    class Connection;

    class Statement : public IStatement
    {
        friend class Cursor;

        Connection* conn;
        std::string query;
        OCIStmt* stmtp;

        class Bind : public RefCounted
        {
          public:
            OCIBind* ptr;
            cxxtools::Dynbuffer<char> data;
            sb2 indicator;
            Datetime datetime;
            Number number;

            Bind()
              : ptr(0),
                indicator(0)
              { }
            void setData(const std::string& value)
            {
              data.reserve(value.size());
              value.copy(data.data(), value.size());
            }
            void setData(const char* value, unsigned size)
            {
              data.reserve(size);
              memcpy(data.data(), value, size);
            }
            void setNull(bool sw = true)
            {
              indicator = sw ? -1 : 0;
            }
        };

        typedef std::map<std::string, SmartPtr<Bind, InternalRefCounted> > BindMapType;
        BindMapType bindMap;

        Bind* getBindPtr(const std::string& col);
        Bind& getBind(const std::string& col)   { return *getBindPtr(col); }

      public:
        /** convenience method */
        void checkError(sword ret, const char* function = 0) const
          { conn->checkError(ret, function); }

        Statement(Connection* conn, const std::string& query);
        ~Statement();

        // methods of IStatement

        void clear();
        void setNull(const std::string& col);
        void setBool(const std::string& col, bool data);
        void setInt(const std::string& col, int data);
        void setUnsigned(const std::string& col, unsigned data);
        void setInt32(const std::string& col, int32_t data);
        void setUnsigned32(const std::string& col, uint32_t data);
        void setInt64(const std::string& col, int64_t data);
        void setUnsigned64(const std::string& col, uint64_t data);
        void setDecimal(const std::string& col, const Decimal& data);
        void setFloat(const std::string& col, float data);
        void setDouble(const std::string& col, double data);
        void setChar(const std::string& col, char data);
        void setString(const std::string& col, const std::string& data);
        void setDate(const std::string& col, const Date& data);
        void setTime(const std::string& col, const Time& data);
        void setDatetime(const std::string& col, const tntdb::Datetime& data);

        size_type execute();
        tntdb::Result select();
        tntdb::Row selectRow();
        tntdb::Value selectValue();
        ICursor* createCursor(unsigned fetchsize);

        // getter
        Connection* getConnection() const     { return conn; }
        OCIStmt* getHandle();
        OCIError* getErrorHandle() const      { return conn->getErrorHandle(); }
    };
  }
}

#endif // TNTDB_ORACLE_SQLSTATEMENT_H
