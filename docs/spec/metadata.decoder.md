## Metadata Decoder

### DecodeMetadata()

~~~~~
void DecodeMetadata() {
  ParseMetadataCount();

  for (i = 0; i < num_att_metadata; ++i) {
    ParseAttributeMetadataId(i);
    DecodeMetadataElement(att_metadata[i]);
  }
  DecodeMetadataElement(file_metadata);
}
~~~~~
{:.draco-syntax}


### ParseMetadataCount()

~~~~~
void ParseMetadataCount() {
  num_att_metadata                                                                    varUI32
}
~~~~~
{:.draco-syntax}


### ParseAttributeMetadataId()

~~~~~
void ParseAttributeMetadataId(index) {
  att_metadata_id[index]                                                              varUI32
}
~~~~~
{:.draco-syntax}


### ParseMetadataElement()

~~~~~
void ParseMetadataElement(metadata) {
  metadata.num_entries                                                                varUI32
  for (i = 0; i < metadata.num_entries; ++i) {
    metadata.key_size[i]                                                              UI8
    metadata.key[i]                                                                   I8[size]
    metadata.value_size[i]                                                            UI8
    metadata.value[i]                                                                 I8[size]
  }
  metadata.num_sub_metadata                                                           varUI32
}
~~~~~
{:.draco-syntax}


### ParseSubMetadataKey()

~~~~~
void ParseSubMetadataKey(metadata, index) {
  metadata.sub_metadata_key_size[index]                                               UI8
  metadata.sub_metadata_key[index]                                                    I8[size]
}
~~~~~
{:.draco-syntax}


### DecodeMetadataElement()

~~~~~
void DecodeMetadataElement(metadata) {
  ParseMetadataElement(metadata);
  for (i = 0; i < metadata.num_sub_metadata; ++i) {
    ParseSubMetadataKey(metadata, i);
    DecodeMetadataElement(metadata.sub_metadata[i]);
  }
}
~~~~~
{:.draco-syntax}
