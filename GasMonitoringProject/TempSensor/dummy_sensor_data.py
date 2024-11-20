import numpy as np
import pandas as pd
from sklearn.preprocessing import OneHotEncoder

# I have four sensor types 
sensor_types = ['MQ-137 Ammonia', 'MQ-4 Methane', 'SCD41 CO2', 'MQ136 H2S']

# Create a OneHotEncoder for the sensor types using sample code from Bruce Scotts ECEN 360
enc = OneHotEncoder(sparse_output=False)

# This function will generate data for each sensor 
def generate_sensor_data(num_rows):
    # generate the random sensor types
    sensors = np.random.choice(sensor_types, num_rows)
    
    # generate realistic sensor readings after doing research on common concentrations found in animal farms
    ammonia_readings = np.random.uniform(5, 200, num_rows)    # Ammonia: 5 to 20 ppm
    methane_readings = np.random.uniform(10, 500, num_rows)   # Methane: 10 to 500 ppm
    co2_readings = np.random.uniform(300, 5000, num_rows)     # CO2: 300 to 5000 ppm
    h2s_readings = np.random.uniform(0.5, 20, num_rows)       # H2S: 0.5 to 20 ppm
    
    # use pandas to create a dataframe
    data = pd.DataFrame({
        'Sensor_Type': sensors,
        'Ammonia_ppm' : ammonia_readings,
        'Methane_ppm': methane_readings,
        'CO2_ppm': co2_readings,
        'H2S_ppm': h2s_readings
    })
    
    return data

# generate sensor data for 1000 readings
sensor_data = generate_sensor_data(1000)

# using onehotencoder to categorize the sensor readings
encoded_sensor_types = enc.fit_transform(sensor_data[['Sensor_Type']])

# convert the sensor types to show in dataframe
encoded_df = pd.DataFrame(encoded_sensor_types, columns=enc.categories_[0])

# Concatenate the encoded sensor types with the original sensor data
final_data = pd.concat([sensor_data, encoded_df], axis=1)

# Save the final data to a CSV file
# final_data.to_csv("/Users/joaquinsalas/Desktop/sensor_data.csv", index=False)

# View the final data
print(final_data.head())