/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 Metrological
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "Module.h"
#include "DataRecord.h"

namespace WPEFramework {

namespace Bluetooth {

namespace AVDTP {

    static constexpr uint8_t PSM = 25;

    class EXTERNAL Payload : public DataRecordBE {
    public:
        using Builder = std::function<void(Payload&)>;
        using Inspector = std::function<void(const Payload&)>;

    public:
        // AVDTP is big-endiand oriented
        using DataRecordBE::DataRecordBE;
        ~Payload() = default;
    }; // class Payload

    class EXTERNAL Signal {
    public:
        enum signalidentifier : uint8_t {
            INVALID                     = 0x00,
            AVDTP_DISCOVER              = 0x01,
            AVDTP_GET_CAPABILITIES      = 0x02,
            AVDTP_SET_CONFIGURATION     = 0x03,
            AVDTP_GET_CONFIGURATION     = 0x04,
            AVDTP_RECONFIGURE           = 0x05,
            AVDTP_OPEN                  = 0x06,
            AVDTP_START                 = 0x07,
            AVDTP_CLOSE                 = 0x08,
            AVDTP_SUSPEND               = 0x09,
            AVDTP_ABORT                 = 0x0A,
            AVDTP_SECURITY_CONTROL      = 0x0B,
            AVDTP_GET_ALL_CAPABILITIES  = 0x0C,
            AVDTP_DELAY_REPORT          = 0x0D,
            END
        };

        enum class errorcode : uint8_t {
            SUCCESS                         = 0x00,

            // Header errors
            BAD_HEADER_FORMAT               = 0x01,

            // Payload format errors
            BAD_LENGTH                      = 0x11,
            BAD_ACP_SEID                    = 0x12,
            SEP_IN_USE                      = 0x13,
            SEP_NOT_IN_USE                  = 0x14,
            BAD_SERV_CATEGORY               = 0x17,
            BAD_PAYLOAD_FORMAT              = 0x18,
            NOT_SUPPORTED_COMMAND           = 0x19,
            INVALID_CAPABILITIES            = 0x1A,

            // Transport service errors
            BAD_RECOVERY_TYPE               = 0x22,
            BAD_MEDIA_TRANSPORT_FORMAT      = 0x23,
            BAD_RECOVERY_FORMAT             = 0x25,
            BAD_ROHC_FORMAT                 = 0x26,
            BAD_CP_FORMAT                   = 0x27,
            BAD_MULTIPLEXING_FORMAT         = 0x28,
            UNSUPPORTED_CONFIGURATION       = 0x29,

            // Procedure errors
            BAD_STATE                       = 0x31,

            // In-house errors
            IN_PROGRESS                     = 0xFE,
            GENERAL_ERROR                   = 0xFF
        };

    protected:
        static constexpr uint8_t INVALID_LABEL = 0xFF;

        enum class messagetype : uint8_t {
            COMMAND             = 0x00,
            GENERAL_REJECT      = 0x01,
            RESPONSE_ACCEPT     = 0x02,
            RESPONSE_REJECT     = 0x03,
            END
        };

        enum class packettype : uint8_t {
            SINGLE      = 0x00,
            START       = 0x01,
            CONTINUE    = 0x02,
            END
        };

    public:
        Signal(const Signal&) = delete;
        Signal& operator=(const Signal&) = delete;

        explicit Signal(const uint16_t bufferSize = 64)
            : _buffer(new uint8_t[bufferSize])
            , _payload(_buffer.get(), bufferSize, 0)
            , _label(INVALID_LABEL)
            , _id(INVALID)
            , _type(messagetype::COMMAND)
            , _errorCode(errorcode::GENERAL_ERROR)
            , _expectedPackets(0)
            , _processedPackets(0)
            , _offset(0)
        {
            ASSERT(bufferSize >= 2);
            ASSERT(_buffer.get() != nullptr);
        }
        ~Signal() = default;

    public:
        uint8_t Label() const {
            return (_label);
        }
        signalidentifier Id() const {
            return (_id);
        }
        messagetype Type() const {
            return (_type);
        }
        errorcode Error() const {
            return (_errorCode);
        }
        bool IsValid() const {
            return ((_label != INVALID_LABEL) && (_id != INVALID) && (_id < signalidentifier::END));
        }
        bool IsComplete() const {
            return (_expectedPackets == _processedPackets);
        }

    public:
        void InspectPayload(const Payload::Inspector& inspectCb) const
        {
            ASSERT(inspectCb != nullptr);

            _payload.Rewind();
            inspectCb(_payload);
        }

    public:
        void Clear()
        {
            _label = INVALID_LABEL;
            _id = INVALID;
            _errorCode = errorcode::GENERAL_ERROR;
            _expectedPackets = 0;
            _processedPackets = 0;
            _payload.Clear();
        }
        void Reload() const
        {
            _expectedPackets = 0;
            _processedPackets = 0;
            _payload.Rewind();
        }

        uint16_t Serialize(uint8_t stream[], const uint16_t length) const;
        uint16_t Deserialize(const uint8_t stream[], const uint16_t length);

    public:
        string AsString() const
        {
#ifdef __DEBUG__
            // Plain lookup, these tables are unlikely to ever change...
            static const char *idLabels[] = {
                "INVALID", "AVDTP_DISCOVER", "AVDTP_GET_CAPABILITIES", "AVDTP_SET_CONFIGURATION",
                "AVDTP_GET_CONFIGURATION", "AVDTP_RECONFIGURE", "AVDTP_OPEN", "AVDTP_START",
                "AVDTP_CLOSE", "AVDTP_SUSPEND", "AVDTP_ABORT", "AVDTP_SECURITY_CONTROL",
                "AVDTP_GET_ALL_CAPABILITIES", "AVDTP_DELAY_REPORT"
            };

            static const char *messageTypeLabels[] = {
                "COMMAND", "GENERAL_REJECT", "RESPONSE_ACCEPT", "RESPONSE_REJECT"
            };

            ASSERT(_id < signalidentifier::END);
            ASSERT(_type <= messagetype::END);

            return Core::Format("signal #%d %s '%s' (%d bytes, %d packets)",
                            _label, messageTypeLabels[static_cast<uint8_t>(_type)],
                            idLabels[static_cast<uint8_t>(_id)],
                            _payload.Length(), _expectedPackets);
#else
            return (Core::Format("signal #%d type %d id %d", _label _type, _id));
#endif
        }

    protected:
        void Set(const uint8_t label, const signalidentifier identifier, const messagetype type)
        {
            _label = label;
            _id = identifier;
            _type = type;
            _offset = 0;
            _expectedPackets = 0;
            _processedPackets = 0;
            _errorCode = errorcode::IN_PROGRESS;
            _payload.Clear();
        }
        void Set(const uint8_t label, const signalidentifier identifier, const messagetype type, const Payload::Builder& buildCb)
        {
            Set(label, identifier, type);

            if (buildCb != nullptr) {
                buildCb(_payload);
            }
        }

    private:
        std::unique_ptr<uint8_t[]> _buffer;
        Payload _payload;
        uint8_t _label;
        signalidentifier _id;
        messagetype _type;
        errorcode _errorCode;
        mutable uint8_t _expectedPackets;
        mutable uint8_t _processedPackets;
        uint16_t _offset;
    }; // class Signal

    class EXTERNAL ClientSocket : public Core::SynchronousChannelType<Core::SocketPort> {
    public:
        static constexpr uint32_t CommunicationTimeout = 1000; /* ms */

    public:
        enum channeltype {
            SIGNALLING,
            TRANSPORT,
            REPORTING,
            RECOVERY,
        };

    public:
        class EXTERNAL Command : public Core::IOutbound, public Core::IInbound {
        public:
            class EXTERNAL Request : public Signal {
            public:
                Request(const Request&) = delete;
                Request& operator=(const Request&) = delete;
                Request()
                    : Signal()
                    , _counter(0xF)
                {
                }
                ~Request() = default;

            public:
                using Signal::Set;

                void Set(const signalidentifier signal)
                {
                    Set(Counter(), signal, messagetype::COMMAND);
                }
                void Set(const signalidentifier signal, const uint8_t acpSeid)
                {
                    ASSERT((acpSeid > 0) && (acpSeid < 0x3F));

                    Set(Counter(), signal, messagetype::COMMAND, [&](Payload& payload) {

                        payload.Push(static_cast<uint8_t>(acpSeid << 2));
                    });
                }
                void Set(const signalidentifier signal, const uint8_t acpSeid, const Payload::Builder& buildCb)
                {
                    ASSERT((acpSeid > 0) && (acpSeid < 0x3F));

                    Set(Counter(), signal, messagetype::COMMAND, [&](Payload& payload) {

                        payload.Push(static_cast<uint8_t>(acpSeid << 2));

                        buildCb(payload);
                    });
                }
                void Set(const signalidentifier signal, const uint8_t acpSeid, const uint8_t intSeid, const Payload::Builder& buildCb)
                {
                    ASSERT((acpSeid > 0) && (acpSeid < 0x3F));
                    ASSERT((intSeid > 0) && (intSeid < 0x3F));

                    Set(Counter(), signal, messagetype::COMMAND, [&](Payload& payload) {

                        payload.Push(static_cast<uint8_t>(acpSeid << 2));
                        payload.Push(static_cast<uint8_t>(intSeid << 2));

                        buildCb(payload);
                    });
                }

            private:
                uint8_t Counter() const
                {
                    _counter = ((_counter + 1) & 0xF);
                    return (_counter);
                }

            private:
                mutable uint8_t _counter;
            }; // class Request

        public:
            class EXTERNAL Response : public Signal {
            public:
                Response(const Response&) = delete;
                Response& operator=(const Response&) = delete;
                Response()
                    : Signal()
                {
                }
                ~Response() = default;
            }; // class Response

        public:
            Command(const Command&) = delete;
            Command& operator=(const Command&) = delete;
            Command(ClientSocket& socket)
                : _request()
                , _response()
                , _socket(socket)
            {
            }
            ~Command() = default;

        public:
            template<typename... Args>
            void Set(const Signal::signalidentifier signal, Args&&... args)
            {
                _status = ~0;
                _response.Clear();
                _request.Set(signal, std::forward<Args>(args)...);
            }

        public:
            Request& Call() {
                return (_request);
            }
            const Request& Call() const {
                return (_request);
            }
            Response& Result() {
                return (_response);
            }
            const Response& Result() const {
                return (_response);
            }
            bool IsAccepted() const {
                return (Result().Error() == Signal::errorcode::SUCCESS);
            }
            bool IsValid() const {
                return (_request.IsValid());
            }

        private:
            void Reload() const override
            {
                _request.Reload();
            }
            uint16_t Serialize(uint8_t stream[], const uint16_t length) const override
            {
                const uint16_t result = _request.Serialize(stream, std::min(_socket.OutputMTU(), length));

                CMD_DUMP("AVTDP client sent", stream, result);

                return (result);
            }
            uint16_t Deserialize(const uint8_t stream[], const uint16_t length) override
            {
                CMD_DUMP("AVTDP client received", stream, length);

                return (_response.Deserialize(stream, length));
            }
            Core::IInbound::state IsCompleted() const override
            {
                return (_response.IsComplete() == true? Core::IInbound::COMPLETED : Core::IInbound::INPROGRESS);
            }

        private:
            uint32_t _status;
            Request _request;
            Response _response;
            ClientSocket& _socket;
        }; // class Command

    public:
        ClientSocket(const ClientSocket&) = delete;
        ClientSocket& operator=(const ClientSocket&) = delete;

        ClientSocket(const Core::NodeId& localNode, const Core::NodeId& remoteNode)
            : Core::SynchronousChannelType<Core::SocketPort>(SocketPort::SEQUENCED, localNode, remoteNode, 2048, 2048)
            , _adminLock()
            , _omtu(0)
            , _type(SIGNALLING)
        {
        }
        ClientSocket(const SOCKET& connector, const Core::NodeId& remoteNode)
            : Core::SynchronousChannelType<Core::SocketPort>(SocketPort::SEQUENCED, connector, remoteNode, 2048, 2048)
            , _adminLock()
            , _omtu(0)
            , _type(SIGNALLING)
        {
        }
        ~ClientSocket() = default;

    public:
        uint16_t OutputMTU() const {
            return (_omtu);
        }
        channeltype Type() const {
            return (_type);
        }

    private:
        virtual void Operational(const bool upAndRunning) = 0;

        void StateChange() override
        {
            Core::SynchronousChannelType<Core::SocketPort>::StateChange();

            if (IsOpen() == true) {
                struct l2cap_options options{};
                socklen_t len = sizeof(options);

                ::getsockopt(Handle(), SOL_L2CAP, L2CAP_OPTIONS, &options, &len);

                ASSERT(options.omtu <= SendBufferSize());
                ASSERT(options.imtu <= ReceiveBufferSize());

                _omtu = options.omtu;

                TRACE(Trace::Information, (_T("AVDTP channel input MTU: %d, output MTU: %d"), options.imtu, options.omtu));

                Operational(true);
            }
            else {
                Operational(false);
            }
        }
        uint16_t Deserialize(const uint8_t stream[] VARIABLE_IS_NOT_USED, const uint16_t length) override
        {
            if (length != 0) {
                // We received data we did not request...
                TRACE_L1("Unexpected data for deserialization [%d bytes]", length);
                CMD_DUMP("AVTDP client received unexpected", stream, length);
            }

            return (0);
        }

    private:
        Core::CriticalSection _adminLock;
        uint16_t _omtu;

    protected:
        channeltype _type;
    }; // class ClientSocket

    class EXTERNAL ServerSocket : public ClientSocket {
    public:
        class EXTERNAL ResponseHandler {
        public:
            ResponseHandler(const ResponseHandler&) = default;
            ResponseHandler& operator=(const ResponseHandler&) = default;
            ~ResponseHandler() = default;

            ResponseHandler(const std::function<void(const Payload::Builder&)>& acceptor,
                            const std::function<void(const Signal::errorcode, const uint8_t)>& rejector)
                : _acceptor(acceptor)
                , _rejector(rejector)
            {
            }

        public:
            void operator ()(const Payload::Builder& buildCb = nullptr) const
            {
                _acceptor(buildCb);
            }
            void operator ()(const Signal::errorcode result = Signal::errorcode::SUCCESS, const uint8_t data = 0) const
            {
                if (result == Signal::errorcode::SUCCESS) {
                    _acceptor(nullptr);
                }
                else {
                    _rejector(result, data);
                }
            }

        private:
            std::function<void(const Payload::Builder& buildCb)> _acceptor;
            std::function<void(const Signal::errorcode, const uint8_t data)> _rejector;
        }; // class ResponseHandler

    public:
        ServerSocket(const ServerSocket&) = delete;
        ServerSocket& operator=(const ServerSocket&) = delete;

        ServerSocket(const Core::NodeId& localNode, const Core::NodeId& remoteNode)
            : ClientSocket(localNode, remoteNode)
            , _request()
            , _response(*this)

        {
        }
        ServerSocket(const SOCKET& connector, const Core::NodeId& remoteNode)
            : ClientSocket(connector, remoteNode)
            , _request()
            , _response(*this)
        {
        }
        ~ServerSocket() = default;

    private:
        class EXTERNAL Request : public Signal {
        public:
            Request(const Request&) = delete;
            Request& operator=(const Request&) = delete;
            Request()
                : Signal()
            {
            }
            ~Request() = default;
        };

        class EXTERNAL Response : public Signal, public Core::IOutbound {
        public:
            Response(const Response&) = delete;
            Response& operator=(const Response&) = delete;
            Response(ServerSocket& socket)
                : Signal()
                , _socket(socket)
            {
            }
            ~Response() = default;

        public:
            void Reload() const override
            {
                Signal::Reload();
            }
            uint16_t Serialize(uint8_t stream[], const uint16_t length) const override
            {
                const uint16_t result = Signal::Serialize(stream, std::min(_socket.OutputMTU(), length));

                CMD_DUMP("AVTDP server sent", stream, result);

                return (result);
            }

        public:
            void Accept(const uint8_t label, const signalidentifier identifier, const Payload::Builder& buildCb = nullptr)
            {
                Set(label, identifier, messagetype::RESPONSE_ACCEPT, buildCb);
            }

        public:
            void Reject(const uint8_t label, const signalidentifier identifier)
            {
                Set(label, identifier, messagetype::GENERAL_REJECT);
            }
            void Reject(const uint8_t label, const signalidentifier identifier, const errorcode code, const uint8_t data = 0)
            {
                ASSERT(code != Signal::errorcode::SUCCESS);

                if ((identifier == AVDTP_SET_CONFIGURATION) || (identifier == AVDTP_RECONFIGURE)
                        || (identifier == AVDTP_START) || (identifier == AVDTP_SUSPEND)) {

                    Set(label, identifier, messagetype::RESPONSE_REJECT, [&](Payload& buffer) {
                        // Data may be zero if it doesn't fit the error code.
                        buffer.Push(data);
                        buffer.Push(code);
                    });
                }
                else {
                    Set(label, identifier, messagetype::RESPONSE_REJECT, [&](Payload& buffer) {
                        buffer.Push(code);
                    });
                }
            }

        private:
            ServerSocket& _socket;
        };

    public:
        void Type(const channeltype type)
        {
            _type = type;

#ifdef __DEBUG__
            static const char* labels[] = { "signalling", "transport", "reporting", "recovery" };
            ASSERT(type < RECOVERY);
            TRACE_L1("AVDTP: Changed channel type to: %s", labels[type]);
#endif
        }

    public:
        uint16_t Deserialize(const uint8_t stream[], const uint16_t length) override
        {
            // This is an AVDTP request from a client.
            CMD_DUMP("AVDTP server received", stream, length);

            uint16_t result = 0;

            if (_type == SIGNALLING) {
                result = _request.Deserialize(stream, length);

                if (_request.IsComplete() == true) {
                    Received(_request, _response);
                    _request.Clear();
                }
            }
            else if (_type == TRANSPORT) {
                OnPacket(stream, length);
                result = length;
            }
            else {
                ASSERT(!"Invalid socket type");
            }

            return (result);
        }

    private:
        void Received(const Request& request, Response& response)
        {
            if (request.IsValid() == true) {
                OnSignal(request, ResponseHandler(
                    [&](const Payload::Builder& buildCb) {
                        TRACE_L1("AVDTP server: accepting %s", request.AsString().c_str());
                        response.Accept(request.Label(), request.Id(), buildCb);
                    },
                    [&](const Signal::errorcode result, const uint8_t data) {
                        TRACE_L1("AVDTP server: rejecting %s, reason: %d, data 0x%02x", request.AsString().c_str(), result, data);
                        response.Reject(request.Label(), request.Id(), result, data);
                    }));
            }
            else {
                // Totally no clue what this signal is, reply with general reject.
                TRACE_L1("AVDTP server: unknown signal received [%02x]", request.Id());
                response.Reject(request.Label(), request.Id());
            }

            Send(CommunicationTimeout, response, nullptr, nullptr);
        }

    protected:
        virtual void OnSignal(const Signal& request, const ResponseHandler& handler) = 0;
        virtual void OnPacket(const uint8_t stream[], const uint16_t length) = 0;

    private:
        Request _request;
        Response _response;
    }; // class ServerSocket

} // namespace AVDTP

} // namespace Bluetooth

}

