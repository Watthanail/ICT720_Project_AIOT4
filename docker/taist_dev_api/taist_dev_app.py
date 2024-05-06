from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from typing import List, Optional
from datetime import datetime, timedelta
import mysql.connector

# Establish MySQL connection
mydb = mysql.connector.connect(
    host="103.253.73.68",
    user="taistdev",
    password="taistdev",
    database="taistdb"
)

# Create cursor
mycursor = mydb.cursor()

class DeviceRegistration(BaseModel):
    device_name: str
    device_addr: str
    branch_name: str
    status: Optional[str] = None
    created_time: Optional[datetime] = None  # Make it Optional to allow None
    update_time: Optional[datetime] = None   # Make it Optional to allow None
    Latitude: float
    Longtitude: float
    status_registor: Optional[str] = None
    create_registor: Optional[datetime] = None  # Make it Optional to allow None

class Registor(BaseModel):
    device_name: str
    device_addr: str
    branch_name: str
    Latitude: float
    Longtitude: float

app = FastAPI()

@app.get("/devices", response_model=List[DeviceRegistration])
def get_devices():
    # Commit any pending transactions to ensure data consistency
    mydb.commit()
    # Fetch devices from MySQL
    mycursor.execute("SELECT * FROM dev_registor_db")
    devices = mycursor.fetchall()
    device_list = []
    for device in devices:
        device_obj = DeviceRegistration(
            device_name=device[0],
            device_addr=device[1],
            branch_name=device[2],
            status=device[3],
            created_time=device[4],
            update_time=device[5],
            Latitude=device[6],
            Longtitude=device[7],
            status_registor=device[8],
            create_registor=device[9]
        )
        device_list.append(device_obj)
    return device_list

@app.post('/register_device')
async def register_device(register: Registor):
    # Insert data into MySQL
    sql = "INSERT INTO dev_registor_db (device_name, device_addr, branch_name, Latitude, Longtitude) VALUES (%s, %s, %s, %s, %s)"
    values = (
        register.device_name,
        register.device_addr,
        register.branch_name,
        register.Latitude,
        register.Longtitude
    )
    mycursor.execute(sql, values)
    mydb.commit()

    return {"message": "Device registered successfully"}

@app.delete('/delete/{device_addr}')
async def delete_device(device_addr: str):
    # Delete data from MySQL
    delete_device_query = "DELETE FROM dev_registor_db WHERE device_addr = %s"
    mycursor.execute(delete_device_query, (device_addr,))
    mydb.commit()
    # Check if the device was found and deleted
    if mycursor.rowcount == 0:
        raise HTTPException(status_code=404, detail="Device not found")


    delete_query = "DELETE FROM user_registor_db WHERE device_addr = %s"
    mycursor.execute(delete_query, (device_addr,))
    mydb.commit()



    return {"message": "Device deleted successfully"}
