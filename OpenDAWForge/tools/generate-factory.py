#!/usr/bin/env python3
"""Deterministic original CC0 factory audio; no external samples or presets."""
from pathlib import Path
import hashlib, json, math, random, struct, wave
root=Path(__file__).resolve().parents[1]/'resources/factory'
root.mkdir(parents=True,exist_ok=True)
rate=24000
entries=[]
def audio(kind,duration,pitch=60,seed=42):
    rng=random.Random(seed);f=440*2**((pitch-69)/12);result=[]
    for i in range(int(duration*rate)):
        t=i/rate;noise=rng.uniform(-1,1)
        if kind=='kick':v=math.sin(2*math.pi*(48*t+3*(1-math.exp(-t*24))))*math.exp(-t*10)
        elif kind=='snare':v=(noise*.7+math.sin(2*math.pi*180*t)*.3)*math.exp(-t*17)
        elif kind=='hat':v=noise*math.exp(-t*55)*.55
        elif kind=='clap':v=noise*math.exp(-t*15)*(1 if t<.035 or .045<t<.075 or t>.085 else .1)
        elif kind=='tom':v=math.sin(2*math.pi*(110*t+1.5*(1-math.exp(-t*16))))*math.exp(-t*9)
        elif kind=='rim':v=(math.sin(2*math.pi*800*t)+math.sin(2*math.pi*1230*t))*.5*math.exp(-t*80)
        elif kind=='pluck':v=sum(math.sin(2*math.pi*f*k*t)*math.exp(-t*(2+k)) / k for k in range(1,6))*.5
        elif kind=='ep':v=(math.sin(2*math.pi*f*t)+.25*math.sin(2*math.pi*f*2*t)+.12*math.sin(2*math.pi*f*4*t))*math.exp(-t*2)*.65
        elif kind=='pad':v=(math.sin(2*math.pi*f*t)+.4*math.sin(2*math.pi*f*1.005*t)+.2*math.sin(2*math.pi*f*2*t))*.4*min(1,t/.3)*min(1,(duration-t)/.35)
        elif kind=='vocal-a':v=sum(math.sin(2*math.pi*f*k*t)*math.exp(-((f*k-730)/470)**2)/k for k in range(1,18))*1.7*min(1,t/.1)*min(1,(duration-t)/.2)
        elif kind=='vocal-o':v=sum(math.sin(2*math.pi*f*k*t)*math.exp(-((f*k-400)/260)**2)/k for k in range(1,18))*1.5*min(1,t/.1)*min(1,(duration-t)/.2)
        elif kind=='riser':v=(noise*.45+math.sin(2*math.pi*(80*t+220*t*t))*.25)*(t/duration)*min(1,(duration-t)/.08)
        elif kind=='impact':v=(noise*.35+math.sin(2*math.pi*55*t))*.65*math.exp(-t*5)
        elif kind=='downlift':v=noise*.65*(1-t/duration)*min(1,t/.015)
        else:v=math.sin(2*math.pi*(900*t+200*math.exp(-t*6)))*math.exp(-t*6)*.8
        result.append(max(-.9,min(.9,v))*min(1,i/40))
    return result
def write(name,values):
    path=root/(name+'.wav')
    with wave.open(str(path),'wb') as w:
        w.setparams((1,2,rate,0,'NONE','not compressed'));w.writeframes(b''.join(struct.pack('<h',int(max(-.99,min(.99,v))*32767)) for v in values))
    return path.name,hashlib.sha256(path.read_bytes()).hexdigest()
def entry(id,name,kind,category,instrument,genre='Electronic',mood='Energetic',bpm=0,source=None,params=None,description='Original LS Factory sound'):
    filename,digest=write(id,source) if source is not None else ('','')
    e=dict(id=id,name=name,kind=kind,category=category,instrument=instrument,genre=genre,mood=mood,bpm=bpm,tags=[category.lower(),genre.lower(),mood.lower(),'factory'],description=description,license='CC0-1.0',tier='free',pack='LS Factory Essentials',file=filename,sha256=digest,resource=filename.replace('.','_').replace('-',''),parameters=params or dict(type=0,volume=.65,pan=0,cutoff=.6,delay=0,reverb=0,compression=0,drive=0))
    entries.append(e)
for k,name in [('kick','Deep Kick'),('snare','Paper Snare'),('hat','Closed Hat'),('clap','Room Clap'),('tom','Low Tom'),('rim','Wood Rim')]:entry(k,name,'one-shot','Drums',k,source=audio(k,.8),params=dict(type=4,volume=.7,cutoff=1,pan=0,delay=0,reverb=0,compression=0,drive=0))
for k in ['kick','snare','hat']:
    for variation in range(2):
        source=audio(k,.8,seed=90+variation);filtered=[];last=0
        for v in source:
            last=.25*v+.75*last
            filtered.append(last*.75 if variation==0 else math.tanh(v*1.6)*.65)
        entry(k+('-soft' if variation==0 else '-punch'),k.title()+(' Soft' if variation==0 else ' Punch'),'one-shot','Drums',k,source=filtered,params=dict(type=4,volume=.7,cutoff=1,pan=0,delay=0,reverb=0,compression=0,drive=0))
for k,name in [('pluck','Glass Pluck'),('ep','Electric Keys'),('pad','Air Pad'),('vocal-a','Synthetic Ah Texture'),('vocal-o','Synthetic Oh Texture'),('riser','Noise Riser'),('impact','Soft Impact'),('downlift','Noise Downlift'),('laser','Laser Shot')]:
    category='Keys' if k in ['pluck','ep'] else 'Pads' if k=='pad' else 'Vocals' if k.startswith('vocal') else 'FX'
    entry(k,name,'sample',category,k,genre='Ambient' if category in ['Pads','Vocals'] else 'Electronic',mood='Dreamy' if category in ['Pads','Vocals','Keys'] else 'Energetic',source=audio(k,3 if k in ['pad','riser'] else 2),params=dict(type=4,volume=.6,cutoff=1,pan=0,delay=0,reverb=.15 if category in ['Pads','Keys'] else 0,compression=0,drive=0),description='Original synthesized texture, not a recorded human vocal' if category=='Vocals' else 'Original synthesized LS Factory sample')
for category,names in [('Bass',['Round Sub','Warm Bass','Driven Bass','Dub Pulse']),('Synths',['Bright Saw','Soft Lead','Wide Echo','Night Arp']),('Keys',['Mellow Synth Keys','Bell Keys','Short Keys']),('Pads',['Warm Sustain','Space Pad','Cloud Pad']),('Drums',['Native Kick','Native Snare','Native Hat'])]:
    for n,name in enumerate(names):
        typ=1+n if category=='Drums' else 0
        params=dict(type=typ,volume=.6,pan=0,cutoff=(.13+.12*n if category=='Bass' else .3+.16*n),delay=.22 if 'Echo' in name or 'Pulse' in name else 0,reverb=.35 if category=='Pads' else .1 if category=='Keys' else 0,compression=.15 if category=='Bass' else 0,drive=.3 if 'Driven' in name else 0)
        entry('preset-'+name.lower().replace(' ','-'),name,'preset',category,'Native '+('drum' if category=='Drums' else 'synth'),genre='Ambient' if category=='Pads' else 'Electronic',mood='Dreamy' if category=='Pads' else 'Energetic',params=params)
for genre,bpm in [('House',120),('Techno',140),('Hip-hop',90),('Ambient',90)]:
    for variation in range(2):
        length=4*60/bpm;values=[0.0]*int(rate*length)
        events=[('kick',x) for x in ([0,1,2,3] if genre in ['House','Techno'] else [0,1.5,2.5])]+[('snare',1),('snare',3)]+[('hat',x*.5) for x in range(8)]
        if variation:events += [('rim',.75),('rim',2.75)]
        for kind,beat in events:
            offset=int(beat*60/bpm*rate)
            for i,v in enumerate(audio(kind,.5,seed=42+variation)):
                if offset+i<len(values):values[offset+i]+=v*.42
        entry(f'loop-{genre.lower()}-{bpm}-{variation}',f'{genre} Drums {bpm} / '+('B' if variation else 'A'),'loop','Drums','Drum loop',genre=genre,mood='Relaxed' if genre in ['Ambient','Hip-hop'] else 'Energetic',bpm=bpm,source=values,params=dict(type=4,volume=.65,cutoff=1,pan=0,delay=0,reverb=0,compression=0,drive=0))
(root/'manifest.json').write_text(json.dumps(dict(schema=1,pack='LS Factory Essentials',entries=entries),indent=2)+'\n')
(root/'LICENSE.txt').write_text('LS Factory Essentials\nAll audio and presets in this pack are original synthesized content, dedicated to CC0 1.0 Universal.\nNo FL Studio assets, factory samples or presets are included.\nhttps://creativecommons.org/publicdomain/zero/1.0/\n')
print(f'Generated {len(entries)} original factory entries')
