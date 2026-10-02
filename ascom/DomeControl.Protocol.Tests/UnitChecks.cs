using System;
using System.Collections.Generic;
using System.Globalization;

namespace DomeControl.Protocol.Tests
{
    /// <summary>
    /// The protocol layer against a scripted transport: what goes on the wire, and what a bad
    /// reply does. The end-to-end checks against the real firmware live in E2E.cs.
    /// </summary>
    internal static class UnitChecks
    {
        public static IEnumerable<(string Name, Action Body)> All()
        {
            yield return ("the command frame ends with #", () =>
            {
                var transport = new LoopbackTransport().Script("OK\r\n");
                new DomeProtocol(transport).GoForward();
                Assert.Equal("GOF#", transport.Sent[0], "frame");
            });

            yield return ("a raw frame keeps the caller's own terminator", () =>
            {
                var transport = new LoopbackTransport().Script("OK\r\n");
                new DomeProtocol(transport).CommandString("GOF#", true);
                Assert.Equal("GOF#", transport.Sent[0], "frame");
            });

            yield return ("stale bytes are dropped before every transaction", () =>
            {
                var transport = new LoopbackTransport().Script("OK\r\n", "OK\r\n");
                var protocol = new DomeProtocol(transport);
                protocol.MotorStop();
                protocol.FindCenter();
                Assert.Equal(2, transport.ClearBuffersCalls, "ClearBuffers calls");
            });

            yield return ("every command issues its own string", () =>
            {
                var transport = new LoopbackTransport().Script("OK\r\n", "OK\r\n", "OK\r\n", "OK\r\n",
                                                               "0\r\n", "1\r\n", "0\r\n");
                var protocol = new DomeProtocol(transport);
                protocol.GoForward();
                protocol.GoReverse();
                protocol.MotorStop();
                protocol.FindCenter();
                protocol.GetEncoderValue();
                protocol.IsOnCenter();
                protocol.IsMoving();
                var expected = new[] { "GOF#", "GOR#", "ST#", "FC#", "GEV#", "IOC#", "IM#" };
                Assert.Equal(string.Join(",", expected), string.Join(",", transport.Sent), "frames");
            });

            yield return ("GoTo carries a signed position", () =>
            {
                var transport = new LoopbackTransport().Script("OK\r\n", "OK\r\n");
                var protocol = new DomeProtocol(transport);
                protocol.GoTo(-42);
                protocol.GoTo(42);
                Assert.Equal("GT-42#,GT42#", string.Join(",", transport.Sent), "frames");
            });

            yield return ("OK is accepted, anything else is a protocol error", () =>
            {
                var good = new LoopbackTransport().Script("OK\r\n");
                new DomeProtocol(good).MotorStop();

                var bad = new LoopbackTransport().Script("Unrecognized command: ST#\r\n");
                DomeProtocolException error = Assert.Throws<DomeProtocolException>(
                    () => new DomeProtocol(bad).MotorStop(), "unknown command reply");
                Assert.That(error.Message.Contains("ST") && error.Message.Contains("Unrecognized"),
                            "the message names the command and the reply: " + error.Message);
            });

            yield return ("a numeric reply is parsed as Int16", () =>
            {
                var transport = new LoopbackTransport().Script("-42\r\n");
                Assert.Equal((short)-42, new DomeProtocol(transport).GetEncoderValue(), "GEV#");
            });

            yield return ("a 0/1 reply becomes a bool", () =>
            {
                var transport = new LoopbackTransport().Script("1\r\n", "0\r\n");
                var protocol = new DomeProtocol(transport);
                Assert.Equal(true, protocol.IsOnCenter(), "IOC# 1");
                Assert.Equal(false, protocol.IsOnCenter(), "IOC# 0");
            });

            yield return ("a non-numeric reply is a protocol error", () =>
            {
                var transport = new LoopbackTransport().Script("OK\r\n");
                DomeProtocolException error = Assert.Throws<DomeProtocolException>(
                    () => new DomeProtocol(transport).GetEncoderValue(), "textual reply to GEV#");
                Assert.That(error.InnerException is FormatException, "the FormatException is kept");
            });

            yield return ("a reply outside Int16 is a protocol error, not a wrap", () =>
            {
                var transport = new LoopbackTransport().Script("40000\r\n");
                DomeProtocolException error = Assert.Throws<DomeProtocolException>(
                    () => new DomeProtocol(transport).GetEncoderValue(), "overflowing reply");
                Assert.That(error.InnerException is OverflowException, "the OverflowException is kept");
            });

            yield return ("0 is the home azimuth", () =>
            {
                Assert.Close(180.0, new DomeProtocol(new LoopbackTransport()).EncoderValueToAzimuth(0),
                             1e-9, "encoder 0");
            });

            yield return ("encoder steps turn into azimuth degrees", () =>
            {
                var protocol = new DomeProtocol(new LoopbackTransport());
                Assert.Close(180.0 + 0.533, protocol.EncoderValueToAzimuth(1), 1e-9, "encoder 1");
                Assert.Close(180.0 - 0.533, protocol.EncoderValueToAzimuth(-1), 1e-9, "encoder -1");
            });

            yield return ("an azimuth above 360 wraps back into [0, 360)", () =>
            {
                var protocol = new DomeProtocol(new LoopbackTransport());
                double wrapped = protocol.EncoderValueToAzimuth(short.MaxValue);
                Assert.That(wrapped >= 0.0 && wrapped < 360.0, "in range, got " + wrapped);
                double below = protocol.EncoderValueToAzimuth(short.MinValue);
                Assert.That(below >= 0.0 && below < 360.0, "in range, got " + below);
            });

            yield return ("an azimuth takes the short way round", () =>
            {
                var protocol = new DomeProtocol(new LoopbackTransport());
                Assert.Equal((short)0, protocol.AzimuthToEncoderValue(180.0), "home");
                Assert.Equal((short)1, protocol.AzimuthToEncoderValue(180.0 + 0.533), "one step forward");
                Assert.Equal((short)-1, protocol.AzimuthToEncoderValue(180.0 - 0.533), "one step back");
                // 359 is +179 from home; the other way round would be -181, so the short path wins.
                Assert.Equal((short)336, protocol.AzimuthToEncoderValue(359.0), "359 degrees");
                // 1 is -179 from home, which is already the short way round.
                Assert.Equal((short)-336, protocol.AzimuthToEncoderValue(1.0), "1 degree");
            });

            yield return ("azimuth and encoder values survive a round trip", () =>
            {
                var protocol = new DomeProtocol(new LoopbackTransport());
                for (short encoder = -100; encoder <= 100; encoder++)
                {
                    double azimuth = protocol.EncoderValueToAzimuth(encoder);
                    short back = protocol.AzimuthToEncoderValue(azimuth);
                    Assert.Equal(encoder, back,
                        string.Format(CultureInfo.InvariantCulture, "encoder {0}", encoder));
                }
            });
        }
    }
}