#version 450
layout(set=0,binding=0,std430) readonly buffer Frame {uint pixels[];} image;
layout(set=0,binding=1,std430) readonly buffer Mark {uint pixels[];} mark;
layout(push_constant) uniform Settings {uvec4 frame;vec4 tone;vec4 effects;vec4 style;uvec4 markSize;} s;
layout(location=0) out vec4 outputColor;
vec3 linearize(vec3 v){return mix(v/12.92,pow((v+.055)/1.055,vec3(2.4)),greaterThan(v,vec3(.04045)));}
float markPixel(ivec2 p){if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,ivec2(s.markSize.xy))))return 0.;return float(mark.pixels[p.y*int(s.markSize.z)+p.x]&255u)/255.;}
float markInk(vec2 p){ivec2 base=ivec2(floor(p));vec2 t=fract(p);return mix(mix(markPixel(base),markPixel(base+ivec2(1,0)),t.x),mix(markPixel(base+ivec2(0,1)),markPixel(base+ivec2(1,1)),t.x),t.y);}
void main(){
 uvec2 p=uvec2(gl_FragCoord.xy);vec2 resolution=vec2(s.frame.xy);vec2 uv=gl_FragCoord.xy/resolution;
 vec4 packed=unpackUnorm4x8(image.pixels[p.y*s.frame.x+p.x]);vec3 color=s.frame.z!=0?packed.rgb:packed.bgr;
 float luma=dot(color,vec3(.2126,.7152,.0722));vec3 filtered=color;int preset=int(s.effects.w+.5);
 if(preset==1)filtered=(mix(vec3(luma),color,1.35)-.5)*1.12+.5;
 else if(preset==2){filtered=(color-.5)*1.12+.5;filtered+=vec3(-.025,.015,.045)*(1.-luma)+vec3(.045,.012,-.025)*luma;filtered=mix(vec3(luma),filtered,.85);}
 else if(preset==3)filtered=color*vec3(1.10,1.02,.90);
 else if(preset==4)filtered=color*vec3(.90,1.02,1.10);
 else if(preset==5)filtered=vec3(luma);
 else if(preset==6)filtered=vec3(dot(color,vec3(.393,.769,.189)),dot(color,vec3(.349,.686,.168)),dot(color,vec3(.272,.534,.131)));
 color=mix(color,filtered,s.style.x);color*=s.tone.x;color=(color-.5)*s.tone.y+.5;
 luma=dot(color,vec3(.2126,.7152,.0722));color=mix(vec3(luma),color,s.tone.z);color+=s.tone.w*vec3(1.,.15,-1.);
 vec2 center=(uv-.5)*2.;float vignette=smoothstep(.2,1.5,dot(center,center));color*=1.-vignette*s.effects.x;
 // Stable spatial grain: freezes with the image instead of flickering in captures.
 float noise=fract(sin(dot(vec2(p),vec2(12.9898,78.233)))*43758.5453)-.5;color+=noise*s.effects.y;
 float border=s.effects.z==0.?0.:max(0.,(resolution.y-resolution.x/s.effects.z)*.5);
 if(gl_FragCoord.y<border||gl_FragCoord.y>=resolution.y-border)color=vec3(0.);
 if(s.style.z>0.){
  float scale=resolution.y/2160.*s.style.w;vec2 size=vec2(s.markSize.xy)*scale;float margin=resolution.y*.025;
  int corner=int(s.style.y+.5);vec2 origin=vec2((corner==1||corner==3)?resolution.x-size.x-margin:margin,corner>=2?resolution.y-size.y-margin:margin);
  ivec2 q=ivec2(floor((gl_FragCoord.xy-origin)/scale));
  if(all(greaterThanEqual(q,ivec2(0)))&&all(lessThan(q,ivec2(s.markSize.xy)))){
   vec3 lettering=vec3(float(s.markSize.w>>24),float((s.markSize.w>>16)&255u),float((s.markSize.w>>8)&255u))/255.;
   bool darkInk=dot(lettering,vec3(.2126,.7152,.0722))<.2;
   if((s.markSize.w&255u)==1u){
    vec2 halfSize=vec2(s.markSize.xy)*.5;float radius=min(halfSize.y,36.);vec2 d=abs(vec2(q)+.5-halfSize)-(halfSize-vec2(radius));float distance=length(max(d,0.))+min(max(d.x,d.y),0.)-radius;
    float glass=1.-smoothstep(-.5,.5,distance);color=mix(color,darkInk?vec3(.96,.97,.99):vec3(.025,.032,.045),glass*s.style.z*.55);
    float rim=glass*(1.-smoothstep(.5,1.5,-distance));color=mix(color,darkInk?vec3(0.):vec3(1.),rim*s.style.z*.18);
   }
   vec2 samplePoint=(gl_FragCoord.xy-origin)/scale-.5;float ink=markInk(samplePoint);float shadowInk=markInk(samplePoint-vec2(4.));

   vec3 shadowColor=darkInk?vec3(.95):vec3(0.);
   color=mix(color,shadowColor,shadowInk*s.style.z*.65);color=mix(color,lettering,ink*s.style.z);
  }
 }
 color=clamp(color,0.,1.);if(s.frame.w!=0)color=linearize(color);outputColor=vec4(color,1.);
}
