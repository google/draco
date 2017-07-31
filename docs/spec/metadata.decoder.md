## Metadata Decoder

### DecodeGeometryMetadata()

~~~~~
DecodeGeometryMetadata(metadata) {
  num_att_metadata                                                                   varUI32
  for (i = 0; i < num_att_metadata; ++i) {
    att_id                                                                           varUI32
    att_metadata = new AttributeMetadata(att_id)
    DecodeMetadata(att_metadata)
    metadata.att_metadata.push_back(att_metadata)
  }
  DecodeMetadata(metadata)
}
~~~~~
{:.draco-syntax}


### DecodeMetadata()

~~~~~
DecodeMetadata(metadata) {
  num_entries                                                                        varUI32
  for (i = 0; i < num_entries; ++i) {
    DecodeEntry(metadata)
  }
  num_sub_metadata                                                                   varUI32
  for (i = 0; i < num_sub_metadata; ++i) {
    size                                                                             UI8
    name                                                                             size * UI8
    temp_metadata = new Metadata()
    DecodeMetadata(temp_metadata)
    metadata.sub_metadata[name] = temp_metadata
  }
}
~~~~~
{:.draco-syntax}


### DecodeEntry()

~~~~~
DecodeEntry(metadata) {
  size                                                                               UI8
  name                                                                               size * UI8
  size                                                                               varUI32
  value                                                                              size * UI8
  metadata.entries.insert(name, value)
}
~~~~~
{:.draco-syntax}
