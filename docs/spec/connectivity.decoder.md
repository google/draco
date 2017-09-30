
## Connectivity Decoder

### DecodeConnectivityData()

~~~~~
void DecodeConnectivityData() {
  if (encoder_method == MESH_SEQUENTIAL_ENCODING)
    DecodeSequentialConnectivityData();
  else
    DecodeEdgebreakerConnectivityData();
}

~~~~~
{:.draco-syntax }
