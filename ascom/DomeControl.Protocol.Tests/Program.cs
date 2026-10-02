using System;
using System.Collections.Generic;

namespace DomeControl.Protocol.Tests
{
    /// <summary>
    /// Console runner for the protocol checks. No test framework on purpose: `dotnet build` has to
    /// work with no network and no NuGet restore, because this is the only C# check that can run
    /// without Windows (AGENTS.md §6). Exit code 0 when nothing failed.
    /// </summary>
    internal static class Program
    {
        private static int Main()
        {
            List<(string Name, Action Body)> checks = new List<(string, Action)>(UnitChecks.All());

            int failures = 0;
            foreach ((string name, Action body) in checks)
            {
                try
                {
                    body();
                    Console.WriteLine("  PASS  " + name);
                }
                catch (Exception e)
                {
                    failures++;
                    Console.WriteLine("  FAIL  " + name + " -- " + e.Message);
                }
            }

            Console.WriteLine();
            Console.WriteLine(string.Format("{0} checks, {1} failures", Assert.Checks, failures));
            return failures > 0 ? 1 : 0;
        }
    }
}
