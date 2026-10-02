using System;
using System.Collections.Generic;

namespace DomeControl.Protocol.Tests
{
    /// <summary>
    /// Console runner for the protocol checks. No test framework on purpose: `dotnet build` has to
    /// work with no network and no NuGet restore, because this is the only C# check that can run
    /// without Windows (AGENTS.md §6). Exit code 0 when nothing failed.
    ///
    /// A check that cannot run here reports SKIP. A skipped check has verified nothing, so it is
    /// never counted as a pass.
    /// </summary>
    internal static class Program
    {
        private static int Main()
        {
            var checks = new List<(string Name, Action Body)>(UnitChecks.All());
            checks.AddRange(E2EChecks.All());

            int failures = 0;
            int skipped = 0;
            try
            {
                foreach ((string name, Action body) in checks)
                {
                    try
                    {
                        body();
                        Console.WriteLine("  PASS  " + name);
                    }
                    catch (SkippedException e)
                    {
                        skipped++;
                        Console.WriteLine("  SKIP  " + name + " -- " + e.Message);
                    }
                    catch (Exception e)
                    {
                        failures++;
                        Console.WriteLine("  FAIL  " + name + " -- " + e.Message);
                    }
                }
            }
            finally
            {
                E2EChecks.Shutdown();
            }

            Console.WriteLine();
            Console.WriteLine(string.Format("{0} checks, {1} failures, {2} skipped",
                                           Assert.Checks, failures, skipped));
            if (failures > 0)
            {
                return 1;
            }
            if (skipped > 0)
            {
                Console.WriteLine("skipped checks verified nothing; do not report their subject as verified.");
            }
            return 0;
        }
    }
}