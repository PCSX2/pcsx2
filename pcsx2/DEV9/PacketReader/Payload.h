#pragma once

namespace PacketReader
{
	class Payload
	{
	public:
		virtual int GetLength() { return 0; }
		virtual void WriteBytes(u8* buffer, int* offset) {}
		virtual ~Payload() {}
	};

	//Pointer to bytes not owned by class
	class PayloadPtr : public Payload
	{
	public:
		u8* data;

	private:
		int length;

	public:
		PayloadPtr(u8* ptr, int len)
		{
			data = ptr;
			length = len;
		}
		virtual int GetLength()
		{
			return length;
		}
		virtual void WriteBytes(u8* buffer, int* offset)
		{
			//If buffer & data point to the same location
			//Then no copy is needed
			if (data == buffer)
				return;

			memcpy(buffer, data, length);
			*offset += length;
		}
	};
} // namespace PacketReader
