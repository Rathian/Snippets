#define NOMINMAX

#include <Ws2tcpip.h>
#include <iphlpapi.h>

// Need to link with Iphlpapi.lib
#pragma comment(lib, "iphlpapi.lib")

#include <chrono>
#include <thread>
#include <algorithm>

int main()
{
	DWORD retVal = 0;
	MIB_IF_ROW2 ifRow;
	memset(&ifRow, 0, sizeof(ifRow));
	PMIB_IF_TABLE2 ifTable;
	retVal = GetIfTable2(&ifTable);
	if (retVal == NO_ERROR)
	{
		ULONG64  value = 0;
		// Under all Adapters, find the "primary" one
		for (ULONG i = 0; i < ifTable->NumEntries; i++)
		{
			const auto& adapter = ifTable->Table[i];
			ULONG64 inout = adapter.InOctets + adapter.OutOctets;
			if (inout)
			{
				// Primary adapter prob. has the highest usage
				if (inout > value)
				{
					value = inout;
					ifRow.InterfaceIndex = adapter.InterfaceIndex;
				}
				// Prob. a duplicate, the main adapter has a lower index
				else if (inout == value)
				{
					ifRow.InterfaceIndex = std::min(ifRow.InterfaceIndex, adapter.InterfaceIndex);
				}
			}
		}
		FreeMibTable(ifTable);
	}

	bool init = false;
	ULONG64 inBytesLast = 0;
	ULONG64 outBytesLast = 0;
	auto begin = std::chrono::high_resolution_clock::now();
	typedef std::chrono::duration<float> fsec;
	for (;;)
	{
		retVal = GetIfEntry2(&ifRow);
		if (retVal == NO_ERROR)
		{
			auto end = std::chrono::high_resolution_clock::now();
			if (init)
			{
				wprintf(L"%s: ", ifRow.Alias);
				wprintf(L"%.1fKbps down - ", float((ifRow.InOctets - inBytesLast) / (1024 / 8)) / std::chrono::duration_cast<fsec>(end - begin).count());
				wprintf(L"%.1fKbps up\n", float((ifRow.OutOctets - outBytesLast) / (1024 / 8)) / std::chrono::duration_cast<fsec>(end - begin).count());
			}
			inBytesLast = ifRow.InOctets;
			outBytesLast = ifRow.OutOctets;
			init = true;
			begin = std::chrono::high_resolution_clock::now();
		}
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}