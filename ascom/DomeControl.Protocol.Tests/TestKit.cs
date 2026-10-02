using System;
using System.Collections.Generic;
using System.Globalization;

namespace DomeControl.Protocol.Tests
{
    /// <summary>Assertion helpers; a failure throws and the runner turns it into a red line.</summary>
    internal static class Assert
    {
        public static int Checks { get; private set; }

        public static void That(bool condition, string what)
        {
            Checks++;
            if (!condition)
            {
                throw new AssertionException(what);
            }
        }

        public static void Equal(object expected, object actual, string what)
        {
            Checks++;
            if (!Equals(expected, actual))
            {
                throw new AssertionException(
                    string.Format(CultureInfo.InvariantCulture, "{0}: expected {1}, got {2}",
                                  what, expected, actual));
            }
        }

        public static void Close(double expected, double actual, double tolerance, string what)
        {
            Checks++;
            if (Math.Abs(expected - actual) > tolerance)
            {
                throw new AssertionException(
                    string.Format(CultureInfo.InvariantCulture, "{0}: expected {1} +/- {2}, got {3}",
                                  what, expected, tolerance, actual));
            }
        }

        public static T Throws<T>(Action action, string what) where T : Exception
        {
            Checks++;
            try
            {
                action();
            }
            catch (T expected)
            {
                return expected;
            }
            catch (Exception other)
            {
                throw new AssertionException(
                    string.Format("{0}: expected {1}, got {2}", what, typeof(T).Name,
                                  other.GetType().Name));
            }
            throw new AssertionException(
                string.Format("{0}: expected {1}, nothing was thrown", what, typeof(T).Name));
        }
    }

    internal sealed class AssertionException : Exception
    {
        public AssertionException(string message) : base(message)
        {
        }
    }

    /// <summary>
    /// Thrown by a check that cannot run here (a missing build, an absent tool). The runner prints
    /// it as SKIP and never counts it as a pass.
    /// </summary>
    internal sealed class SkippedException : Exception
    {
        public SkippedException(string reason) : base(reason)
        {
        }
    }

    /// <summary>
    /// A transport that records what was sent and replays scripted replies, so the protocol can be
    /// tested without a port, a firmware or Windows.
    /// </summary>
    internal sealed class LoopbackTransport : IDomeTransport
    {
        private readonly Queue<string> replies = new Queue<string>();

        public List<string> Sent { get; } = new List<string>();

        public int ClearBuffersCalls { get; private set; }

        public LoopbackTransport Script(params string[] scripted)
        {
            foreach (string reply in scripted)
            {
                replies.Enqueue(reply);
            }
            return this;
        }

        public void ClearBuffers()
        {
            ClearBuffersCalls++;
        }

        public void Transmit(string frame)
        {
            Sent.Add(frame);
        }

        public string ReceiveTerminated(string terminator)
        {
            if (replies.Count == 0)
            {
                throw new InvalidOperationException("no scripted response left for " + Sent[Sent.Count - 1]);
            }
            string reply = replies.Dequeue();
            if (!reply.EndsWith(terminator, StringComparison.Ordinal))
            {
                throw new InvalidOperationException(
                    string.Format("scripted response {0} does not end with {1}", reply, terminator));
            }
            return reply;
        }

        public void Dispose()
        {
        }
    }
}