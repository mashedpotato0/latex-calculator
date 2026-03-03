import { useState, useEffect, useRef, useMemo, useCallback } from "react";

/* ═══════════════════════════════ JS FALLBACK (For Plotter Only) ════════════ */
// We retain a lightweight JS evaluator strictly so the canvas plotter
// remains snappy (60fps), while symbolic logic calls the C++ API.

const NUM_CONSTS = { pi:Math.PI, e:Math.E, phi:(1+Math.sqrt(5))/2, tau:2*Math.PI, inf:Infinity };

function tokenizeJS(src){
  const toks=[];let i=0;
  while(i<src.length){
    if(/\s/.test(src[i])){i++;continue;}
    if(/[0-9]/.test(src[i])||(src[i]==='.'&&/[0-9]/.test(src[i+1]||''))){
      let n='';while(i<src.length&&/[0-9.]/.test(src[i]))n+=src[i++];
      toks.push({t:'NUM',v:parseFloat(n)});
    }else if(/[a-zA-Z_]/.test(src[i])){
      let id='';while(i<src.length&&/[a-zA-Z_0-9]/.test(src[i]))id+=src[i++];
      toks.push({t:'ID',v:id});
    }else{
      const c=src[i++];
      const M={'+':'PLUS','-':'MINUS','*':'STAR','/':'SLASH','^':'CARET','(':'LP',')':'RP'};
      toks.push({t:M[c]||'UNK',v:c});
    }
  }
  toks.push({t:'EOF',v:''});return toks;
}

function parseJS(src){
  try{
    const toks=tokenizeJS(src);let pos=0;
    const cur=()=>toks[pos]; const eat=()=>toks[pos++];
    const expect=t=>{if(cur().t!==t)throw new Error(`Expected ${t}`);return eat();};
    function expr(){return add();}
    function add(){let l=mul();while(cur().t==='PLUS'||cur().t==='MINUS'){const op=eat().v;l={t:'bin',op,l,r:mul()};}return l;}
    function mul(){let l=unary();while(cur().t==='STAR'||cur().t==='SLASH'){const op=eat().v;l={t:'bin',op,l,r:unary()};}return l;}
    function unary(){if(cur().t==='MINUS'){eat();return {t:'neg',a:unary()};}if(cur().t==='PLUS'){eat();return unary();}return pow();}
    function pow(){const b=primary();if(cur().t==='CARET'){eat();return {t:'bin',op:'^',l:b,r:unary()};}return b;}
    function primary(){
      const c=cur();
      if(c.t==='NUM'){eat();return {t:'num',v:c.v};}
      if(c.t==='LP'){eat();const e=expr();expect('RP');return e;}
      if(c.t==='ID'){
        eat();
        if(cur().t==='LP'){eat();const arg=expr();expect('RP');return {t:'func',name:c.v,arg};}
        return {t:'var',name:c.v};
      }
      throw new Error(`Unexpected: "${c.v}"`);
    }
    return {ok:true, ast:expr()};
  }catch(e){return {ok:false, err:e.message};}
}

function evalJS(ast, xVal){
  if(!ast) return 0;
  if(ast.t==='num') return ast.v;
  if(ast.t==='var') {
    if(ast.name==='x') return xVal;
    if(NUM_CONSTS[ast.name]) return NUM_CONSTS[ast.name];
    return 0;
  }
  if(ast.t==='neg') return -evalJS(ast.a, xVal);
  if(ast.t==='bin'){
    const l=evalJS(ast.l, xVal), r=evalJS(ast.r, xVal);
    if(ast.op==='+') return l+r; if(ast.op==='-') return l-r;
    if(ast.op==='*') return l*r; if(ast.op==='/') return l/r;
    if(ast.op==='^') return Math.pow(l,r);
  }
  if(ast.t==='func'){
    const a=evalJS(ast.arg, xVal);
    const fns = { sin:Math.sin, cos:Math.cos, tan:Math.tan, exp:Math.exp, log:Math.log, ln:Math.log, sqrt:Math.sqrt, abs:Math.abs };
    return fns[ast.name] ? fns[ast.name](a) : 0;
  }
  return 0;
}

/* ═══════════════════════════════ KATEX ════════════════════════════════════ */
function useKaTeX(){
  const[loaded,setLoaded]=useState(!!window.katex);
  useEffect(()=>{
    if(window.katex){setLoaded(true);return;}
    const lnk=document.createElement('link');lnk.rel='stylesheet';lnk.href='https://cdnjs.cloudflare.com/ajax/libs/KaTeX/0.16.9/katex.min.css';document.head.appendChild(lnk);
    const sc=document.createElement('script');sc.src='https://cdnjs.cloudflare.com/ajax/libs/KaTeX/0.16.9/katex.min.js';sc.onload=()=>setLoaded(true);document.head.appendChild(sc);
  },[]);
  return loaded;
}

function KTX({latex,display=false,cls=''}){
  const ref=useRef(null);const loaded=useKaTeX();
  useEffect(()=>{
    if(!ref.current)return;
    if(!window.katex){ref.current.textContent=latex;return;}
    try{window.katex.render(latex,ref.current,{displayMode:display,throwOnError:false,strict:false});}
    catch{if(ref.current)ref.current.textContent=latex;}
  },[latex,display,loaded]);
  return <span ref={ref} className={cls}/>;
}

/* ═══════════════════════════════ CANVAS PLOTTER ══════════════════════════ */
function Plotter(){
  const canvasRef=useRef(null);
  const[plotExpr,setPlotExpr]=useState('sin(x)');
  const[xMin,setXMin]=useState('-6.28');
  const[xMax,setXMax]=useState('6.28');
  const[yMin,setYMin]=useState('-2');
  const[yMax,setYMax]=useState('2');
  const[err,setErr]=useState('');

  const draw=useCallback(()=>{
    const canvas=canvasRef.current;if(!canvas)return;
    const ctx=canvas.getContext('2d');
    const W=canvas.width,H=canvas.height;
    const xlo=parseFloat(xMin),xhi=parseFloat(xMax),ylo=parseFloat(yMin),yhi=parseFloat(yMax);
    if(isNaN(xlo)||isNaN(xhi)||isNaN(ylo)||isNaN(yhi))return;
    const toX=x=>(x-xlo)/(xhi-xlo)*W;
    const toY=y=>(1-(y-ylo)/(yhi-ylo))*H;

    ctx.fillStyle='#080c18';ctx.fillRect(0,0,W,H);
    ctx.strokeStyle='#1a2235';ctx.lineWidth=1;
    const xStep=(xhi-xlo)/10,yStep=(yhi-ylo)/8;
    for(let x=Math.ceil(xlo/xStep)*xStep;x<=xhi;x+=xStep){ctx.beginPath();ctx.moveTo(toX(x),0);ctx.lineTo(toX(x),H);ctx.stroke();}
    for(let y=Math.ceil(ylo/yStep)*yStep;y<=yhi;y+=yStep){ctx.beginPath();ctx.moveTo(0,toY(y));ctx.lineTo(W,toY(y));ctx.stroke();}

    ctx.strokeStyle='#2a3a5a';ctx.lineWidth=1.5;
    if(xlo<=0&&0<=xhi){ctx.beginPath();ctx.moveTo(toX(0),0);ctx.lineTo(toX(0),H);ctx.stroke();}
    if(ylo<=0&&0<=yhi){ctx.beginPath();ctx.moveTo(0,toY(0));ctx.lineTo(W,toY(0));ctx.stroke();}

    const parsed=parseJS(plotExpr);
    if(!parsed.ok){setErr(parsed.err);return;}
    setErr('');
    const N=W*2;
    ctx.strokeStyle='#f59e0b';ctx.lineWidth=2;ctx.beginPath();
    let started=false;
    for(let i=0;i<=N;i++){
      const x=xlo+(xhi-xlo)*i/N;
      let y;
      try{y=evalJS(parsed.ast, x);}catch{started=false;continue;}
      if(!isFinite(y)||isNaN(y)){started=false;continue;}
      const px=toX(x),py=toY(y);
      if(!started){ctx.moveTo(px,py);started=true;}else ctx.lineTo(px,py);
    }
    ctx.stroke();
  },[plotExpr,xMin,xMax,yMin,yMax]);

  useEffect(()=>{draw();},[draw]);

  return(
    <div style={{padding:12}}>
      <div style={{display:'flex',gap:6,marginBottom:8,flexWrap:'wrap'}}>
        <input value={plotExpr} onChange={e=>setPlotExpr(e.target.value)}
          placeholder="f(x), e.g. sin(x)*cos(2x)"
          style={{flex:1,minWidth:160,background:'#0d1120',border:'1px solid #1e2a40',borderRadius:4,padding:'6px 10px',color:'#e2e8f0',fontFamily:'inherit',fontSize:12}}/>
      </div>
      {err&&<div style={{fontSize:11,color:'#f87171',marginBottom:6}}>⚠ {err}</div>}
      <canvas ref={canvasRef} width={620} height={300} style={{width:'100%',height:300,borderRadius:6,border:'1px solid #1a2035',display:'block'}}/>
    </div>
  );
}

const inputStyle={background:'#0d1120',border:'1px solid #1e2a40',borderRadius:4,padding:'5px 6px',color:'#94a3b8',fontFamily:'inherit',fontSize:12,outline:'none'};

/* ═══════════════════════════════ APP ══════════════════════════════════════ */
export default function App(){
  const[input,setInput]=useState('');
  const[diffVar,setDiffVar]=useState('x');
  const[intVar,setIntVar]=useState('x');
  const[history,setHistory]=useState([]);
  const[activePanel,setActivePanel]=useState('calc'); 
  const[histFilter,setHistFilter]=useState('all'); 
  const[loading,setLoading]=useState(false);
  const katexLoaded=useKaTeX();
  const inputRef=useRef(null);

  // Call the Node.js API to run our C++ backend
  const invokeCppBackend = async (op, variable) => {
    if(!input.trim()) return;
    setLoading(true);
    try {
      const response = await fetch('http://localhost:3000/api/math', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ op, var: variable, expr: input.trim() })
      });
      const data = await response.json();
      
      if(data.error) {
         addEntry(op, `\\text{${input}}`, `\\text{Error: ${data.error}}`);
      } else {
         // Data successfully routed through ast.cpp simplifying and parser
         let prefix = '';
         if (op === 'eval') prefix = '= ';
         if (op === 'diff') prefix = `\\frac{d}{d${variable}} \\rightarrow `;
         if (op === 'int') prefix = `\\int d${variable} \\rightarrow `;
         addEntry(op, `\\text{${input}}`, `${prefix} ${data.result}`);
      }
    } catch (e) {
      addEntry(op, `\\text{${input}}`, `\\text{Error: Cannot reach C++ Backend}`);
    } finally {
      setLoading(false);
    }
  };

  function addEntry(op,inputLx,resultLx){
    setHistory(h=>[{op,inputLx,resultLx,id:Date.now()},...h.slice(0,79)]);
  }

  function handleInputKey(e){
    if(e.key==='Enter'&&!e.shiftKey){e.preventDefault(); invokeCppBackend('eval', '');}
  }

  const filteredHistory=histFilter==='all'?history:history.filter(e=>e.op===histFilter);

  const opColor={eval:'#60a5fa',diff:'#34d399',int:'#c084fc'};
  const opBg={eval:'#0d1e30',diff:'#0d1e18',int:'#180d28'};
  const opLabel={eval:'EVAL',diff:'DIFF',int:'INT'};

  return(
    <div style={{minHeight:'100vh',background:'#060910',color:'#dde1ed',fontFamily:'"JetBrains Mono","Fira Code",Consolas,monospace',display:'flex',flexDirection:'column'}}>
      <style>{`
        @import url('https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@300;400;500;600&display=swap');
        *{box-sizing:border-box;margin:0;padding:0;}
        .katex{font-size:1.05em!important;}.katex-display{margin:0!important;}
        .abtn{transition:all .15s;}.abtn:hover{filter:brightness(1.2);transform:translateY(-1px);}.abtn:active{transform:translateY(1px);}
        .ftab{transition:all .12s;border-radius:3px;}.ftab.on{background:#1a2845;color:#f59e0b;}.ftab:not(.on):hover{background:#111828;color:#7a8fad;}
        input,textarea{outline:none;}textarea:focus,input:focus{border-color:#f59e0b!important;}
        .iarea:focus-within{border-color:#f59e0b!important;box-shadow:0 0 0 2px rgba(245,158,11,.1)!important;}
      `}</style>

      {/* ── Header ───────────────────────────────────────────────────────── */}
      <header style={{background:'#080b16',borderBottom:'1px solid #141c2e',padding:'8px 20px',display:'flex',alignItems:'center',gap:14,userSelect:'none'}}>
        <div style={{display:'flex',alignItems:'baseline',gap:6}}>
          <span style={{color:'#f59e0b',fontWeight:700,fontSize:17}}>SYME</span>
        </div>
        <div style={{width:1,height:18,background:'#1a2035'}}/>
        <span style={{fontSize:10,color:'#2e3f5a',letterSpacing:'.06em'}}>C++ SYMBOLIC BACKEND</span>
        <div style={{marginLeft:'auto',display:'flex',gap:6}}>
          {['calc','plot'].map(p=>(
            <button key={p} className={`abtn ftab${activePanel===p?' on':''}`}
              onClick={()=>setActivePanel(p)}
              style={{background:'none',border:'none',cursor:'pointer',padding:'4px 12px',fontSize:11,color:activePanel===p?'#f59e0b':'#3d4f6e',fontFamily:'inherit',letterSpacing:'.04em'}}>
              {p==='calc'?'CALCULATOR':'PLOTTER'}
            </button>
          ))}
        </div>
      </header>

      {/* ── Main ─────────────────────────────────────────────────────────── */}
      <div style={{flex:1,display:'grid',gridTemplateColumns:'360px 1fr',overflow:'hidden',height:'calc(100vh - 41px)'}}>

        {/* ── LEFT PANEL ─────────────────────────────────────────────────── */}
        <div style={{borderRight:'1px solid #141c2e',display:'flex',flexDirection:'column',overflow:'hidden',background:'#070a14'}}>

          {activePanel==='calc'&&<>
            <div style={{padding:10,flex:'0 0 auto'}}>
              <div className="iarea" style={{background:'#0c1020',border:'1px solid #172035',borderRadius:6,overflow:'hidden'}}>
                <textarea ref={inputRef} value={input}
                  onChange={e=>setInput(e.target.value)}
                  onKeyDown={handleInputKey}
                  placeholder="enter expression... (e.g. sin(x)^2 + x * cos(x))"
                  style={{width:'100%',background:'transparent',border:'none',color:'#e2e8f0',fontFamily:'inherit',fontSize:12.5,padding:'9px 11px',resize:'none',height:70,lineHeight:1.65}}/>
              </div>
            </div>

            {/* Action buttons */}
            <div style={{padding:'0 10px 8px',display:'flex',gap:6,flexWrap:'wrap',alignItems:'center'}}>
              <button className="abtn" onClick={() => invokeCppBackend('eval', '')} disabled={loading}
                style={{background:'#122030',border:'1px solid #1e3650',borderRadius:5,padding:'7px 14px',cursor:'pointer',color:'#60a5fa',fontFamily:'inherit',fontSize:11.5}}>
                = Eval
              </button>
              <div style={{display:'flex',gap:3,alignItems:'center'}}>
                <button className="abtn" onClick={() => invokeCppBackend('diff', diffVar)} disabled={loading}
                  style={{background:'#0e2018',border:'1px solid #1e4030',borderRadius:5,padding:'7px 12px',cursor:'pointer',color:'#34d399',fontFamily:'inherit',fontSize:11.5}}>
                  d/d
                </button>
                <input value={diffVar} onChange={e=>setDiffVar(e.target.value)} maxLength={8}
                  style={{width:32,...inputStyle,color:'#f59e0b',textAlign:'center'}}/>
              </div>
              <div style={{display:'flex',gap:3,alignItems:'center'}}>
                <button className="abtn" onClick={() => invokeCppBackend('int', intVar)} disabled={loading}
                  style={{background:'#180e28',border:'1px solid #35204a',borderRadius:5,padding:'7px 12px',cursor:'pointer',color:'#c084fc',fontFamily:'inherit',fontSize:11.5}}>
                  ∫ d
                </button>
                <input value={intVar} onChange={e=>setIntVar(e.target.value)} maxLength={8}
                  style={{width:32,...inputStyle,color:'#f59e0b',textAlign:'center'}}/>
              </div>
            </div>

            <div style={{borderTop:'1px solid #141c2e',flex:1,padding:10}}>
                <div style={{fontSize:9.5,color:'#2e3f5a',letterSpacing:'.07em',marginBottom:5}}>QUICK REFERENCE</div>
                <div style={{fontSize:10,color:'#2a3850',lineHeight:1.9}}>
                  <div><span style={{color:'#3d5272'}}>sin(x), \sin{'{x}'}</span> — Supports LaTeX & Math</div>
                  <div><span style={{color:'#3d5272'}}>x^2 + 1/x</span> — Algebra</div>
                  <div><span style={{color:'#3d5272'}}>pi e phi tau inf</span> — Constants</div>
                  <div style={{marginTop:3,color:'#273445'}}>Driven by custom AST C++ backend logic.</div>
                </div>
            </div>
          </>}

          {activePanel==='plot'&&(
            <div style={{flex:1,overflow:'auto'}}>
              <Plotter/>
            </div>
          )}
        </div>

        {/* ── RIGHT PANEL ────────────────────────────────────────────────── */}
        <div style={{display:'flex',flexDirection:'column',overflow:'hidden',background:'#060910'}}>
          <div style={{padding:'7px 14px',borderBottom:'1px solid #141c2e',background:'#080b16',display:'flex',alignItems:'center',gap:8}}>
            <span style={{fontSize:9.5,color:'#2e3f5a',letterSpacing:'.07em',marginRight:4}}>OUTPUT</span>
            {['all','eval','diff','int'].map(f=>(
              <button key={f} className={`abtn ftab${histFilter===f?' on':''}`} onClick={()=>setHistFilter(f)}
                style={{background:'none',border:'none',cursor:'pointer',padding:'3px 9px',fontSize:10,color:histFilter===f?'#f59e0b':'#2e3f5a',fontFamily:'inherit'}}>
                {f==='all'?'ALL':f.toUpperCase()}
              </button>
            ))}
          </div>

          <div style={{flex:1,overflow:'auto',padding:'10px 12px',display:'flex',flexDirection:'column',gap:7}}>
            {filteredHistory.map(entry=>(
              <div key={entry.id} style={{background:opBg[entry.op],border:`1px solid ${entry.op==='eval'?'#122030':entry.op==='diff'?'#0e2a1a':'#1a0e2a'}`,borderRadius:7,overflow:'hidden'}}>
                <div style={{padding:'5px 10px',borderBottom:'1px solid #0d1525',display:'flex',alignItems:'center',gap:7}}>
                  <span style={{fontSize:9,padding:'2px 7px',borderRadius:2,fontWeight:600,color:opColor[entry.op],background:`${opColor[entry.op]}18`}}>
                    {opLabel[entry.op]}
                  </span>
                </div>
                <div style={{padding:'6px 12px 3px',color:'#3d5070',fontSize:11.5,borderBottom:'1px solid #0d1525'}}>
                  <KTX latex={entry.inputLx} display={false}/>
                </div>
                <div style={{padding:'10px 14px',overflowX:'auto'}}>
                  <KTX latex={entry.resultLx} display={true}/>
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
}