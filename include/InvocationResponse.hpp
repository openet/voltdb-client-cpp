/* This file is part of VoltDB.
 * Copyright (C) 2008-2013 VoltDB Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef VOLTDB_INVOCATIONRESPONSE_HPP_
#define VOLTDB_INVOCATIONRESPONSE_HPP_
#include <boost/shared_array.hpp>
#include <vector>
#include <sstream>
#include <boost/shared_ptr.hpp>
#include "ByteBuffer.hpp"
#include "Table.h"
#include <iostream>
namespace voltdb {

enum StatusCode {
    /*
     * Returned when a procedure executes without aborting.
     */
    STATUS_CODE_SUCCESS = 1,

    /*
     * Returned when a procedure throws a VoltAbortException and is rolled back
     */
    STATUS_CODE_USER_ABORT = -1,

    /*
     * Returned when a procedure fails due to something like a constraint violation
     */
    STATUS_CODE_GRACEFUL_FAILURE = -2,

    /*
     * Returned when a procedure invocation fails. This can be because the procedure does not exist
     * or it could be due to a runtime error within VoltDB
     */
    STATUS_CODE_UNEXPECTED_FAILURE = -3,

    /*
     * Returned by the API when the connection to the server that a request was sent to is lost
     */
    STATUS_CODE_CONNECTION_LOST = -4
};

/*
 * Response to a stored procedure invocation. Generated by the API when a response is received from the server
 * or the connection to the server the request was sent to is lost.
 */
class InvocationResponse {
public:

#ifdef SWIG
    %ignore InvocationResponse();
#endif
    /*
     * Default constructor generates an error response indicating the connection
     * to the database was lost
     */
    InvocationResponse() :
        m_clientData(0),
        m_statusCode(voltdb::STATUS_CODE_CONNECTION_LOST),
        m_statusString(std::string("Connection to the database was lost")),
        m_appStatusCode(INT8_MIN),
        m_appStatusString(std::string("")),
        m_clusterRoundTripTime(0),
        m_results() {
    }

#ifdef SWIG
    %ignore InvocationResponse(boost::shared_array<char> data, int32_t length);
#endif
    /*
     * Constructor for taking shared ownership of a message buffer
     * containing a response to a stored procedure invocation
     */
    InvocationResponse(boost::shared_array<char>& data, int32_t length) : m_results(0) {
        SharedByteBuffer buffer(data, length);
        int8_t version = buffer.getInt8();
        assert(version == 0);
        m_clientData = buffer.getInt64();
        int8_t presentFields = buffer.getInt8();
        m_statusCode =  buffer.getInt8();
        bool wasNull = false;
        if ((presentFields & (1 << 5)) != 0) {
            m_statusString = buffer.getString(wasNull);
        }
        m_appStatusCode = buffer.getInt8();
        if ((presentFields & (1 << 7)) != 0) {
            m_appStatusString = buffer.getString(wasNull);
        }
        assert(!wasNull);
        m_clusterRoundTripTime = buffer.getInt32();
        if ((presentFields & (1 << 6)) != 0) {
            int32_t position = buffer.position() + 4;
            buffer.position(position + buffer.getInt32());
        }
        size_t resultCount = static_cast<size_t>(buffer.getInt16());
        m_results.resize(resultCount);
        int32_t startLimit = buffer.limit();
        for (size_t ii = 0; ii < resultCount; ii++) {
            int32_t tableLength = buffer.getInt32();
            assert(tableLength >= 4);
            buffer.limit(buffer.position() + tableLength);
            m_results[ii] = voltdb::Table(buffer.slice());
            buffer.limit(startLimit);
        }
    }

    /*
     * Returns the client data generated by the API on behalf of user. Can be ignored.
     */
    int64_t clientData() const { return m_clientData; }

    /*
     * Status code returned by VoltDB
     */
    int8_t statusCode() const { return m_statusCode; }

    /*
     * Returns true if the status code was success, false otherwise
     */
    bool success() const { return m_statusCode == STATUS_CODE_SUCCESS; }

    /*
     * Returns true if the status code was not success, false otherwise
     */
    bool failure() const { return m_statusCode != STATUS_CODE_SUCCESS; }

    /*
     * Returns a human readable string describing what occured. Will be the empty string if the
     * status code is success
     */
    std::string statusString() const { return m_statusString; }

    /*
     * Return status code set by the application (not Volt) while executing the stored procedure.
     * Default value is -128 if the application does not set the code.
     */
    int8_t appStatusCode() const { return m_appStatusCode; }

    /*
     * Return the status string set by the application (not Volt) while executing the stored procedure.
     * Default value is the empty string.
     */
    std::string appStatusString() const { return m_appStatusString; }

    /*
     * Returns the round trip execution time of the stored procedure as measured by the VoltDB node
     * that initiated the stored procedure invocation.
     */
    int32_t clusterRoundTripTime() const { return m_clusterRoundTripTime; }

    /*
     * Returns a vector of tables containing result data returned by the stored procedure
     */
    std::vector<voltdb::Table> results() const { return m_results; }

    /*
     * Generate a string representation of the contents of the message
     */
    std::string toString() const {
        std::ostringstream ostream;
        ostream << "Status: " << static_cast<int32_t>(statusCode()) << ", " << statusString() <<  std::endl;
        ostream << "App Status: " << static_cast<int32_t>(appStatusCode()) << ", " << appStatusString() << std::endl;
        ostream << "Client Data: " << clientData() << std::endl;
        ostream << "Cluster Round Trip Time: " << clusterRoundTripTime() << std::endl;
        for (size_t ii = 0; ii < m_results.size(); ii++) {
            ostream << "Result Table " << ii << std::endl;
            m_results[ii].toString(ostream, std::string("    "));
        }
        return ostream.str();
    }


    void operator >> (std::ostream &ostream) const {
        ostream.write((const char*)&m_statusCode, sizeof(m_statusCode));
        writeString(ostream, m_statusString);
        ostream.write((const char*)&m_appStatusCode, sizeof(m_appStatusCode));
        writeString(ostream, m_appStatusString);
        ostream.write((const char*)&m_clientData, sizeof(m_clientData));
        ostream.write((const char*)&m_clusterRoundTripTime, sizeof(m_clusterRoundTripTime));
        size_t size = m_results.size();
        ostream.write((const char *)&size, sizeof(size));
        for (size_t ii = 0; ii < m_results.size(); ii++) {
            m_results[ii] >> ostream;
        }
    }

    InvocationResponse(std::istream &istream) {
        istream.read((char *)&m_statusCode, sizeof(m_statusCode));
        m_statusString = readString(istream);
        istream.read((char *)&m_appStatusCode, sizeof(m_appStatusCode));
        m_appStatusString = readString(istream);
        istream.read((char *)&m_clientData, sizeof(m_clientData));
        istream.read((char *)&m_clusterRoundTripTime, sizeof(m_clusterRoundTripTime));
        size_t size;
        istream.read((char *)&size, sizeof(size));
        m_results.resize(size);
        for (size_t ii = 0; ii < size; ii++) {
            m_results[ii] = voltdb::Table(istream);
        }
    }

private:
    static std::ostream &writeString(std::ostream &ostream, const std::string &str) {
        const int32_t size = str.size();
        ostream.write((const char*)&size, sizeof(size));
        if (size != 0) {
            ostream.write(str.data(), size);
        } return ostream;
    }

    std::string readString(std::istream &istream) {
        std::string str;
        int32_t size;
        istream.read((char*)&size, sizeof(size));
        if (size != 0) {
            str.resize(size);
            istream.read((char *)str.data(), size);
        } return str;
    }


private:
    int64_t m_clientData;
    int8_t m_statusCode;
    std::string m_statusString;
    int8_t m_appStatusCode;
    std::string m_appStatusString;
    int32_t m_clusterRoundTripTime;
    std::vector<voltdb::Table> m_results;
};
}

#endif /* VOLTDB_INVOCATIONRESPONSE_HPP_ */
