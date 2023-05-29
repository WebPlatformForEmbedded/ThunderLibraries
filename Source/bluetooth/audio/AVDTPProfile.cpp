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

#include "Module.h"
#include "AVDTPProfile.h"

namespace WPEFramework {

namespace Bluetooth {

namespace AVDTP {

    // Client methods
    // --------------------------------

    uint32_t Client::Discover(const std::function<void(StreamEndPoint&&)>& reportCb) const
    {
        uint32_t result;
        std::list<StreamEndPoint> endpoints;

        _command.Set(Signal::AVDTP_DISCOVER);

        // First issue a DISCOVER signal.
        result = Execute(_command, [&](const Payload& payload) {
            // Pick up end point data...
            while (payload.Available() >= 2) {
                uint8_t data[2];
                payload.Pop(data[0]);
                payload.Pop(data[1]);
                endpoints.emplace_back(data);
            }

            if (payload.Available() != 0) {
                TRACE_L1("Unexpected data in payload!");
            }
        });

        if (result == Core::ERROR_NONE) {
            // Then for each of the stream points discovered issue a GET_CAPABILITIES signal.
            for (auto& sep : endpoints) {
                _command.Set(Signal::AVDTP_GET_CAPABILITIES, sep.Id());

                Execute(_command, [&](const Payload& payload) {
                    // Pick up capability data for each category listed...
                    while (payload.Available() >= 2) {
                        StreamEndPoint::Service::categorytype category{};
                        uint8_t length{};
                        Buffer params;

                        // Capabilities are stored as {category:length:params} triplets.
                        payload.Pop(category);
                        payload.Pop(length);

                        if (length > 0) {
                            payload.Pop(params, length);
                        }

                        sep.Add(StreamEndPoint::capability, category, std::move(params));
                    }

                    if (payload.Available() != 0) {
                        TRACE_L1("Unexpected data in payload!");
                    }
                });

                // Hand this endpoint over...
                reportCb(std::move(sep));
            }
        }

        return (result);
    }

    uint32_t Client::SetConfiguration(const uint8_t id, const uint8_t intId, const StreamEndPoint::ServiceMap& config)
    {
        uint32_t result;

        ASSERT(config.empty() == false);
        ASSERT(intId != 0);

        auto cb = [&](Payload& payload) {
            // For each category set the configuration data...
            for (auto const& entry : config) {
                const StreamEndPoint::Service& service = entry.second;

                ASSERT(service.Params().size() <= 255);

                // Configuration data has same structure as capabilities.
                payload.Push(service.Category());
                payload.Push<uint8_t>(service.Params().size());

                if (service.Params().size() > 0) {
                    payload.Push(service.Params());
                }
            }
        };

        _command.Set(Signal::AVDTP_SET_CONFIGURATION, id, intId, cb);

        result = Execute(_command);

        return (result);
    }

    uint32_t Client::GetConfiguration(const uint8_t id, const std::function<void(const uint8_t, Buffer&&)>& reportCb) const
    {
        ASSERT(id != 0);
        ASSERT(reportCb != nullptr);

        _command.Set(Signal::AVDTP_GET_CONFIGURATION, id);

        return (Execute(_command, [&](const Payload& payload) {

            // For each category get the configuration data...
            while (payload.Available() >= 2) {
                StreamEndPoint::Service::categorytype category{};
                uint8_t length{};
                Buffer data;

                payload.Pop(category);
                payload.Pop(length);

                if (length > 0) {
                    payload.Pop(data, length);
                }

                reportCb(category, std::move(data));
            }

            ASSERT(payload.Available() == 0);
        }));
    }

    /* private */
    uint32_t Client::Execute(AVDTP::ClientSocket::Command& cmd, const Payload::Inspector& inspectorCb) const
    {
        uint32_t result = Core::ERROR_ASYNC_FAILED;

        if (_socket.Exchange(AVDTP::ClientSocket::CommunicationTimeout, cmd, cmd) == Core::ERROR_NONE) {

            if (cmd.IsAccepted() == false) {
                TRACE_L1("Signal %d was rejected! [%d]", cmd.Call().Id() ,cmd.Result().Error());
            }
            else {
                result = Core::ERROR_NONE;

                if (inspectorCb != nullptr) {
                    cmd.Result().InspectPayload(inspectorCb);
                }
            }
        }

        return (result);
    }

    // Server methods
    // --------------------------------

    void Server::OnDiscover(const Handler& Reply)
    {
        // Lists all endpoints offered by the signalling server.
        // This call cannot fail.

        Reply([&](Payload& payload) {
            uint8_t id = 0;

            // Serialize all endpoints...
            while (WithEndpoint(++id, [&](const StreamEndPoint& ep) {
                ep.Serialize(payload);
            }) == true);

            // Must be at least on endpoint configured.
            ASSERT(id > 1);
        });
    }

    void Server::OnGetCapabilities(const uint8_t seid, const Handler& Reply)
    {
        // Retrieves basic capabilities of a particular endpoint.

        if (WithEndpoint(seid, [&](const StreamEndPoint& ep) {
            ASSERT(ep.Capabilities().empty() == false);

            // Serialize this endpoints capabilities...
            Reply([&](Payload& payload) {
                for (auto const& entry : ep.Capabilities()) {
                    const StreamEndPoint::Service& service = entry.second;

                    if (StreamEndPoint::Service::IsBasicCategory(service.Category()) == true) {

                        ASSERT(service.Params().size() <= 255);

                        payload.Push(service.Category());
                        payload.Push<uint8_t>(service.Params().size());

                        if (service.Params().size() > 0) {
                            payload.Push(service.Params());
                        }
                    }
                }
            });
        }) == false) {
            Reply(Signal::errorcode::BAD_ACP_SEID);
        }
    }

    void Server::OnGetAllCapabilities(const uint8_t seid, const Handler& Reply)
    {
        // Retrieves all capabilities of a particular endpoint.

        if (WithEndpoint(seid, [&](const StreamEndPoint& ep) {
            ASSERT(ep.Capabilities().empty() == false);

            // Serialize this endpoints capabilities...
            Reply([&](Payload& payload) {
                for (auto const& entry : ep.Capabilities()) {
                    const StreamEndPoint::Service& service = entry.second;

                    ASSERT(service.Params().size() <= 255);

                    payload.Push(service.Category());
                    payload.Push<uint8_t>(service.Params().size());

                    if (service.Params().size() > 0) {
                        payload.Push(service.Params());
                    }
                }
            });
        }) == false) {
            Reply(Signal::errorcode::BAD_ACP_SEID);
        }
    }

    void Server::OnSetConfiguration(const Signal& signal, const Handler& Reply)
    {
        // Sets configuration of a particular endpoint.
        // Requests the endpoint state change from IDLE to CONFIGURED.

        Signal::errorcode code = Signal::errorcode::SUCCESS;
        uint8_t failedCategory = 0;

        uint8_t acpSeid = 0; // ours
        uint8_t intSeid = 0; // theirs

        Payload config;

        signal.InspectPayload([&](const Payload& payload) {
            if (payload.Available() >= 2) {
                payload.Pop(acpSeid);
                acpSeid >>= 2;

                payload.Pop(intSeid);
                intSeid >>= 2;

                // The rest is the raw config data.
                payload.PopAssign(config, payload.Available());
            }
            else {
                code = Signal::errorcode::BAD_LENGTH;
            }
        });

        if (code == Signal::errorcode::SUCCESS) {
            code = Signal::errorcode::BAD_ACP_SEID;

            WithEndpoint(acpSeid, [&](StreamEndPoint& ep) {
                code = DeserializeConfig(config, ep, failedCategory, [](const StreamEndPoint::Service::categorytype category) {
                    if (StreamEndPoint::Service::IsValidCategory(category) == true) {
                        return (Signal::errorcode::SUCCESS);
                    }
                    else {
                        return (Signal::errorcode::BAD_SERV_CATEGORY);
                    }
                });

                if (code == Signal::errorcode::SUCCESS) {
                    code = ToSignalCode(ep.Configure(intSeid, failedCategory));
                }
            });
        }

        Reply(code, failedCategory);
    }

    void Server::OnReconfigure(const Signal& signal, const Handler& Reply)
    {
        // Sets partial configuration of a particular endpoint.
        // 1) Valid only in OPENED state.
        // 2) Only configuration for the "Application Service" can be changed.

        Signal::errorcode code = Signal::errorcode::SUCCESS;
        uint8_t failedCategory = 0;

        uint8_t seid = 0;

        Payload config;

        signal.InspectPayload([&](const Payload& payload) {
            if (payload.Available() >= 2) {
                payload.Pop(seid);
                seid >>= 2;

                // The rest is the raw config data.
                payload.PopAssign(config, payload.Available());
            }
            else {
                code = Signal::errorcode::BAD_LENGTH;
            }
        });

        if (code == Signal::errorcode::SUCCESS) {
            code = Signal::errorcode::BAD_ACP_SEID;

            WithEndpoint(seid, [&](StreamEndPoint& ep) {
                code = DeserializeConfig(config, ep, failedCategory, [](const StreamEndPoint::Service::categorytype category) {
                    if (StreamEndPoint::Service::IsValidCategory(category) == false) {
                        return (Signal::errorcode::BAD_SERV_CATEGORY);
                    }
                    else if (StreamEndPoint::Service::IsApplicationCategory(category) == false) {
                        return (Signal::errorcode::INVALID_CAPABILITIES);
                    }
                    else {
                        return (Signal::errorcode::SUCCESS);
                    }
                });

                if (code == Signal::errorcode::SUCCESS) {
                    code = ToSignalCode(ep.Reconfigure(failedCategory));
                }
            });
        }

        Reply(code, failedCategory);
    }

    void Server::OnOpen(const uint8_t seid, const Handler& Reply)
    {
        // Requests the endpoint state change from CONFIGURED to OPENED.
        // Once opened the initiator can establish a transport connection.

        Signal::errorcode code = Signal::errorcode::SUCCESS;

        WithEndpoint(seid, [&](StreamEndPoint& ep) {
            code = ToSignalCode(ep.Open());
        });

        Reply(code);
    }

    void Server::OnClose(const uint8_t seid, const Handler& Reply)
    {
        // Requests the endpoint state change to OPENED to CONFIGURED.

        Signal::errorcode code = Signal::errorcode::SUCCESS;

        WithEndpoint(seid, [&](StreamEndPoint& ep) {
            code = ToSignalCode(ep.Close());
        });

        Reply(code);
    }

    void Server::OnStart(const uint8_t seid, const Handler& Reply)
    {
        // Requests the endpoint state change from OPENED to STARTED.

        Signal::errorcode code = Signal::errorcode::SUCCESS;

        WithEndpoint(seid, [&](StreamEndPoint& ep) {
            code = ToSignalCode(ep.Start());
        });

        Reply(code, seid);
    }

    void Server::OnSuspend(const uint8_t seid, const Handler& Reply)
    {
        // Requests the endpoint state change form STARTED to OPENED.

        Signal::errorcode code = Signal::errorcode::SUCCESS;

        WithEndpoint(seid, [&](StreamEndPoint& ep) {
            code = ToSignalCode(ep.Suspend());
        });

        Reply(code, seid);
    }

    void Server::OnAbort(const uint8_t seid, const Handler& Reply)
    {
        // Requests the endpoint state change to IDLE.

        Signal::errorcode code = Signal::errorcode::SUCCESS;

        WithEndpoint(seid, [&](StreamEndPoint& ep) {
            code = ToSignalCode(ep.Abort());
        });

        Reply(code);
    }

    /* private */
    Signal::errorcode Server::DeserializeConfig(const Payload& config, StreamEndPoint& ep, uint8_t& invalidCategory,
                    const std::function<Signal::errorcode(const StreamEndPoint::Service::categorytype)>& Verify)
    {
        // Helper method to deserialize configuration.
        // Used by SetConfigure and Reconfigure, but they accept different subset of categories.

        Signal::errorcode code = Signal::errorcode::SUCCESS;

        while (config.Available() >= 2) {
            StreamEndPoint::Service::categorytype category{};
            config.Pop(category);

            code = Verify(category);

            if (code != Signal::errorcode::SUCCESS) {
                TRACE_L1("Invalid category!");
                invalidCategory = category;
            }
            else {
                Buffer buffer;
                uint8_t length{};

                config.Pop(length);

                if (length > 0) {
                    config.Pop(buffer, length);
                }

                ep.Add(category, std::move(buffer));
            }
        }

        if (config.Available() != 0) {
            TRACE_L1("Unexpected data in payload!");
            code = Signal::errorcode::BAD_LENGTH;
        }

        return (code);
    };

} // namespace AVDTP

} // namespace Bluetooth

}
