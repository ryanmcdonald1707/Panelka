// GLSL 330 ports of the three.js ShaderMaterials.
#pragma once

static const char* VS = R"(#version 330
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;
layout(location=3) in vec3 aCol;
layout(location=4) in vec3 aEmit;
layout(location=5) in vec4 aExt;
uniform mat4 viewMatrix;uniform mat4 projectionMatrix;
uniform vec2 uRes;uniform float uSnap,uAffine,uFogNear,uFogFar,uTime,uLights;
uniform mat4 uShadowMat;
out vec3 vUvw;out vec2 vUvP;out float vDepth;out vec3 vCol;out vec3 vEmit;out float vFog;out float vSnowK;out vec3 vW;out vec3 vN;out vec4 vSC;
void main(){
 vec4 wp=vec4(position,1.0);
 vec4 mv=viewMatrix*wp;
 vec4 p=projectionMatrix*mv;
 if(uSnap>0.5&&p.w>0.0){vec2 g=uRes*0.5;p.xy=floor(p.xy/p.w*g+0.5)/g*p.w;}
 gl_Position=p;
 vec3 n=normalize(normal);
 vN=n;vW=wp.xyz;
 vSC=uShadowMat*vec4(wp.xyz+n*0.2,1.0);
 vCol=aCol;
 float w=mix(1.0,p.w,uAffine);
 vUvw=vec3(uv*w,w);
 vUvP=uv;vDepth=-mv.z;
 float on=step(aExt.x,uLights);
 // a TV-lit window breathes gently rather than jumping about
 float fl=aExt.z>0.0?0.8+0.2*sin(uTime*1.7+aExt.z*917.0)*sin(uTime*4.3+aExt.z*311.0):1.0;
 vEmit=aEmit*on*fl;
 vFog=clamp((length(mv.xyz)*aExt.y-uFogNear)/(uFogFar-uFogNear),0.0,1.0);
 vSnowK=aExt.w;
})";

#define DITHER_GLSL R"(
float b2(vec2 a){a=floor(a);return fract(a.x*0.5+a.y*a.y*0.75);}
float b4(vec2 a){return b2(0.5*a)*0.25+b2(a);}
vec3 quant(vec3 c){if(uQ>0.5){float d=b4(gl_FragCoord.xy)-0.5;c=floor(c*31.0+d+0.5)/31.0;}return c;}
)"

static const char* FS = R"(#version 330
uniform sampler2D uTex,uShadow,uLTex,uCTex;
uniform vec3 uFog,uSunDir,uSunCol,uSky,uGnd,uGridMin,uGridN,uCam;
uniform int uLW,uCW;
uniform float uAffine,uQ,uSnow,uLights,uTime,uCell,uShadowOn,uShadowTexel,uPointOn,uInterior,uGlass;
in vec3 vUvw;in vec2 vUvP;in float vDepth;in vec3 vCol;in vec3 vEmit;in float vFog;in float vSnowK;in vec3 vW;in vec3 vN;in vec4 vSC;
out vec4 fragColor;
)" DITHER_GLSL R"(
vec4 fL(int i){return texelFetch(uLTex,ivec2(i%uLW,i/uLW),0);}
vec4 fC(int i){return texelFetch(uCTex,ivec2(i%uCW,i/uCW),0);}
float sunShadow(){
 vec3 c=vSC.xyz/vSC.w*0.5+0.5;
 if(uShadowOn<0.5||c.x<0.0||c.x>1.0||c.y<0.0||c.y>1.0||c.z>1.0)return 1.0;
 // 3x3 taps a texel apart: softer edges, so they don't crawl as the sun moves
 float z=c.z-0.00012,t=uShadowTexel,s=0.0;
 for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++)s+=step(z,texture(uShadow,c.xy+vec2(x,y)*t).r);
 return s/9.0;
}
vec3 pointLights(vec3 P,vec3 N){
 vec3 acc=vec3(0.0);
 if(uPointOn<0.5)return acc;
 vec3 g=(P-uGridMin)/uCell;
 if(g.x<0.0||g.y<0.0||g.z<0.0||g.x>=uGridN.x||g.y>=uGridN.y||g.z>=uGridN.z)return acc;
 int nx=int(uGridN.x),nz=int(uGridN.z);
 int cell=int(floor(g.x))+int(floor(g.z))*nx+int(floor(g.y))*nx*nz;
 for(int k=0;k<6;k++){
  vec4 ids=fC(cell*6+k);
  for(int j=0;j<4;j++){
   float id=ids[j];
   if(id<0.5)return acc;
   int b=(int(id+0.5)-1)*6;
   vec4 a=fL(b),c=fL(b+1);
   if(c.w>uLights)continue;
   vec3 L=a.xyz-P;float d=length(L);
   if(d>a.w)continue;
   vec4 dr=fL(b+2),ex=fL(b+3);
   // room lights touch only interior surfaces inside their room; world lights only the exterior
   if((ex.y>0.5)!=(uInterior>0.5))continue;
   if(ex.y>0.5){vec3 lo=fL(b+4).xyz,hi=fL(b+5).xyz;if(any(lessThan(P,lo))||any(greaterThan(P,hi)))continue;}
   L/=max(d,0.001);
   float ndl=max(dot(N,L),0.0);
   float spot=dr.w<-1.5?1.0:smoothstep(dr.w,dr.w+0.2,dot(-L,dr.xyz));
   float att=1.0-d/a.w;att*=att;
   float fl=ex.x>0.0?0.8+0.2*sin(uTime*1.7+ex.x*917.0)*sin(uTime*4.3+ex.x*311.0):1.0;
   acc+=c.rgb*(ndl*spot*att*fl);
  }
 }
 return acc;
}
// Real glass (transparent pass): Fresnel reflection of the sky, a sun glint, a faint tint.
vec4 glass(vec3 N,vec4 t,vec3 lit){
 vec3 V=normalize(uCam-vW);
 float ndv=abs(dot(N,V)),fr=0.04+0.96*pow(1.0-ndv,5.0);
 vec3 R=reflect(-V,N);
 vec3 env=mix(uGnd*0.9,uSky*1.3+uFog*0.2,smoothstep(-0.2,0.45,R.y));
 float sh=uShadowOn>0.5?sunShadow():1.0;
 float spec=pow(max(dot(R,normalize(uSunDir)),0.0),90.0)*sh;
 vec3 body=t.rgb*0.22*lit+vec3(0.03,0.05,0.055);
 vec3 c=mix(body,env,clamp(0.3+fr*0.7,0.0,1.0))+uSunCol*spec*2.5;
 float a=clamp(0.24+fr*0.7+spec*0.8+(t.a<0.65?0.08:0.0),0.0,0.94);
 return vec4(mix(c,uFog,vFog),a);
}
void main(){
 // Affine warp only at a distance: near the camera big polygons would smear, so fade to perspective-correct UVs.
 vec2 uv=mix(vUvP,vUvw.xy/vUvw.z,uAffine*smoothstep(4.0,20.0,vDepth));
 vec4 t=texture(uTex,uv);
 if(t.a<0.5)discard;
 vec3 N=normalize(vN);
 if(uGlass>0.5&&!gl_FrontFacing)N=-N; // windows are seen from both sides
 vec3 alb=t.rgb*vCol;
 float sn=uSnow*vSnowK*smoothstep(0.5,0.85,N.y);
 if(sn>0.0){float g=fract(sin(dot(floor(uv*512.0),vec2(12.9898,78.233)))*43758.5453);alb=mix(alb,vec3(0.9,0.92,0.96)*(0.86+0.14*g),sn);}
 float ndl=max(dot(N,uSunDir),0.0);
 float sh=ndl>0.0?sunShadow():0.0;
 vec3 lit=mix(uGnd,uSky,N.y*0.5+0.5)+uSunCol*(ndl*sh)+pointLights(vW,N);
 if(uGlass>0.5&&t.a<0.9){vec4 g=glass(N,t,lit);fragColor=vec4(quant(g.rgb),g.a);return;}
 vec3 c=alb*lit;
 if(t.a<0.9){float on=step(0.01,vEmit.r+vEmit.g+vEmit.b);c=mix(c,vEmit*(0.35+1.3*t.rgb),on);}
 c=mix(c,uFog,vFog);
 fragColor=vec4(quant(c),1.0);
})";

// Shadow depth pass (three's depthMat, DoubleSide, alpha-tested)
static const char* DVS = R"(#version 330
layout(location=0) in vec3 position;
layout(location=2) in vec2 uv;
uniform mat4 uVP;
out vec2 vUv;
void main(){vUv=uv;gl_Position=uVP*vec4(position,1.0);})";
static const char* DFS = R"(#version 330
uniform sampler2D uTex;in vec2 vUv;out vec4 o;
void main(){if(texture(uTex,vUv).a<0.5)discard;o=vec4(1.0);})";

// Sky dome
static const char* SVS = R"(#version 330
layout(location=0) in vec3 position;
uniform mat4 uViewRot;uniform mat4 projectionMatrix;
out vec3 vDir;
void main(){vDir=position;vec4 p=projectionMatrix*uViewRot*vec4(position,1.0);gl_Position=p.xyww;})";
static const char* SFS = R"(#version 330
uniform vec3 uHor,uZen,uSunDir,uSunC,uCloudC;uniform float uCloud,uStars,uTime,uQ,uMoon;
in vec3 vDir;out vec4 fragColor;
)" DITHER_GLSL R"(
float h21(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float vn(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);return mix(mix(h21(i),h21(i+vec2(1.0,0.0)),f.x),mix(h21(i+vec2(0.0,1.0)),h21(i+vec2(1.0,1.0)),f.x),f.y);}
float fbm(vec2 p){float s=0.0,a=0.5;for(int i=0;i<4;i++){s+=a*vn(p);p*=2.03;a*=0.5;}return s;}
void main(){
 vec3 d=normalize(vDir);float h=d.y;
 vec3 c=mix(uHor,uZen,pow(clamp(h,0.0,1.0),0.55));
 if(h<0.0)c=uHor*0.92;
 float sd=dot(d,normalize(uSunDir));
 c+=uSunC*pow(max(sd,0.0),24.0)*0.35*(1.0-uMoon);
 if(sd>0.9975)c=mix(c,uMoon>0.5?vec3(0.9,0.92,1.0):vec3(1.0,0.95,0.8),0.9);
 if(uStars>0.0&&h>0.0){vec3 q=floor(d*260.0);float s=fract(sin(dot(q,vec3(12.9898,78.233,37.719)))*43758.5453);if(s>0.9975)c+=vec3(uStars*0.8);}
 if(h>0.0){vec2 uv=d.xz/(h+0.12);float n=fbm(uv*1.6+vec2(uTime*0.004,uTime*0.002));
  float k=1.0-uCloud*0.8;float cl=smoothstep(k-0.15,k+0.25,n)*smoothstep(0.0,0.18,h);c=mix(c,uCloudC,cl*0.9);}
 fragColor=vec4(quant(c),1.0);
})";

// THREE.PointsMaterial (square points, sizeAttenuation)
static const char* PVS = R"(#version 330
layout(location=0) in vec3 position;
uniform mat4 viewMatrix;uniform mat4 projectionMatrix;uniform float size,scale;
void main(){vec4 mv=viewMatrix*vec4(position,1.0);gl_PointSize=size*(scale/-mv.z);gl_Position=projectionMatrix*mv;})";
static const char* PFS = R"(#version 330
uniform vec3 diffuse;uniform float opacity;out vec4 o;
void main(){o=vec4(diffuse,opacity);})";

// #crt overlay: repeating scanline gradient + inset 18vmin box-shadow
static const char* CRTFS = R"(#version 330
in vec2 fragTexCoord;in vec4 fragColor;out vec4 finalColor;
uniform vec2 uScreen;uniform float uSl;
float erf_(float x){float s=sign(x);x=abs(x);float t=1.0/(1.0+0.3275911*x);
 float y=1.0-(((((1.061405429*t-1.453152027)*t)+1.421413741)*t-0.284496736)*t+0.254829592)*t*exp(-x*x);return s*y;}
float Phi(float x){return 0.5*(1.0+erf_(x*0.70710678));}
void main(){
 vec2 p=vec2(gl_FragCoord.x,uScreen.y-gl_FragCoord.y);
 float a1=mod(p.y,uSl)<uSl*0.38?0.30:0.0;
 float sig=0.18*min(uScreen.x,uScreen.y)*0.5;
 float ix=Phi(p.x/sig)-Phi((p.x-uScreen.x)/sig),iy=Phi(p.y/sig)-Phi((p.y-uScreen.y)/sig);
 float a2=0.7*(1.0-ix*iy);
 finalColor=vec4(0.0,0.0,0.0,a2+a1*(1.0-a2));
})";
