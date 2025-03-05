const express = require("express");
const app = express();
const PORT = 8080;
const IP = '172.31.14.43';
const knex = require("knex")({
  client: "mysql",
  connection: {
    host: "localhost",
    port: 3306,
    user: "root",
    password: "1234",
    database: "fram",
  },
});

// Middleware สำหรับแปลงข้อมูลเป็น JSON
app.use(express.json());

// 📌 บันทึกข้อมูลเซ็นเซอร์จาก ESP32 ลงใน weekly_data
app.post('/upload-data', async (req, res) => {
  try {
    const { temperature, humidity, control_mode } = req.body;
    console.log('sensor data:', req.body);

    if (temperature === undefined || humidity === undefined || control_mode === undefined) {
      return res.status(400).json({ ok: 0, error: "Missing temperature, humidity, or control_mode data" });
    }

    await knex('weekly_data').insert({
      temperature: temperature,
      humidity: humidity,
      control_mode: control_mode,
      timestamp: new Date(),
    });

    res.status(200).json({ ok: 1, message: "Data inserted successfully" });
  } catch (error) {
    console.error("Error inserting data:", error.message);
    res.status(500).json({ ok: 0, error: "Error inserting data" });
  }
});

// 📌 บันทึกการเลือกชนิดเห็ด
app.post('/select-mushroom', async (req, res) => {
  try {
    const { mushroom_id } = req.body;
    console.log('mushroom_choices:', req.body);
    if (mushroom_id === undefined) {
      return res.status(400).json({ error: "Missing mushroom_id" });
    }

    await knex('mushroom_choices').insert({
      mushroom_id: mushroom_id,
    });

    res.status(200).json({ message: "Mushroom selection saved successfully" });
  } catch (error) {
    console.error("Error saving mushroom selection:", error.message);
    res.status(500).json({ error: "Error saving selection" });
  }
});

// 📌 ดึงค่าความชื้นและอุณหภูมิที่ต้องการของเห็ดแต่ละชนิด
app.post('/mushroom-conditions', async (req, res) => {
  try {
    const { mushroom_id } = req.body;
    if (!mushroom_id) {
      return res.status(400).json({ error: "Missing mushroom_id" });
    }

    const result = await knex('mushroom')
      .select('temp_need', 'humi_need')
      .where({ mushroom_id })
      .first();

    if (!result) {
      return res.status(404).json({ error: "Mushroom ID not found" });
    }

    res.status(200).json(result);
  } catch (error) {
    console.error("Error fetching mushroom conditions:", error.message);
    res.status(500).json({ error: "Error fetching data" });
  }
});

// เริ่มต้นเซิร์ฟเวอร์
app.listen(PORT, IP, () => {
  console.log(`Upload Server is running at http://${IP}:${PORT}`);
});
