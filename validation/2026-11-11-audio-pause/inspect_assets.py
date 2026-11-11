import array,binascii,hashlib,json,struct,wave
from pathlib import Path
root=Path('validation/2026-11-11-audio-pause')
meta=json.loads((root/'session/prepared.json').read_text())
config=Path(meta['user_data'])/'config/settings.ecfg'
data=config.read_bytes()
assert data[:8]==b'ECFG\x07\x00\x00\x00'
assert binascii.crc32(data[:-4])==struct.unpack('<I',data[-4:])[0]
assert hashlib.sha256(data).hexdigest()==next(f['sha256'] for f in meta['copied_user_files'] if f['path']=='config/settings.ecfg')
master,music,effects=struct.unpack_from('<fff',data,8)
clips={}
for name in ['pickup','door','music','machine']:
    path=Path('assets/sounds')/(name+'.wav')
    with wave.open(str(path),'rb') as wav:
        assert wav.getsampwidth()==2 and wav.getframerate()==48000
        channels=wav.getnchannels();frames=wav.getnframes()
        pcm=array.array('h',wav.readframes(frames))
    assert struct.pack('=h',1)==b'\x01\x00'
    clips[name]=dict(sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        channels=channels,frames=frames,seconds=frames/48000,peak=max(abs(v) for v in pcm)/32768)
bound=master*(music*clips['music']['peak']+effects*(.65*clips['pickup']['peak']+.6*clips['door']['peak']+.4*clips['machine']['peak']))
assert bound<1
out=dict(kind='offline asset/config inspection, not a recording of the reported artifact',
    clips=clips,initial_and_final_config_sha256=hashlib.sha256(data).hexdigest(),
    gains=dict(master=master,music=music,effects=effects),
    single_pickup_door_and_two_loops_per_channel_absolute_sum_bound=bound,
    limits='Bound assumes recorded gains and at most one voice of each asset. It does not exclude multiple overlapping effects, downstream distortion, transient controls or transport artifacts. No cause is assigned.')
(root/'asset-inspection.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(out,indent=2))
