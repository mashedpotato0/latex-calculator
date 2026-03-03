const express = require('express');
const cors = require('cors');
const { execFile } = require('child_process');
const path = require('path');

const app = express();
app.use(cors());
app.use(express.json());

// Determine the executable name based on the OS
const symcalcPath = path.join(__dirname, process.platform === 'win32' ? 'symcalc.exe' : 'symcalc');

app.post('/api/math', (req, res) => {
    const { op, var: variable, expr } = req.body;

    if (!op || !expr) {
        return res.status(400).json({ error: 'Missing operation or expression' });
    }

    const varArg = variable || 'x';

    // Execute the compiled C++ binary in "exec" mode
    execFile(symcalcPath, ['exec', op, varArg, expr], (error, stdout, stderr) => {
        if (error) {
            console.error('Execution error:', stderr || error.message);
            return res.status(500).json({ error: 'Engine failed to process expression' });
        }

        const output = stdout.trim();
        
        // Handle custom engine errors passed via stdout
        if (output.startsWith('ERR:')) {
            return res.status(400).json({ error: output.substring(4) });
        }

        res.json({ result: output });
    });
});

const PORT = 3000;
app.listen(PORT, () => {
    console.log(`C++ Symbolic Backend API running on http://localhost:${PORT}`);
});